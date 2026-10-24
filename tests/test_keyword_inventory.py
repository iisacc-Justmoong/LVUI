"""Check source classification and drift detection with isolated source fixtures."""
import importlib.util
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('keyword_inventory', ROOT / 'tools/update_lvrs_keywords.py')
inventory = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inventory)


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='keyword-audit-', dir=ROOT / 'build')
        self.root = Path(self.temp.name)
        self.write('src/qml/Fixture.qml', 'Item {\n    component InnerVisual: Rectangle {}\n'
                   '    property string note: "component FakeVisual: Item {}"\n}\n')
        self.write('src/qml/Service.qml', 'QtObject {}\n')
        self.write('src/qml/CMakeLists.txt', 'set(FILES src/qml/Fixture.qml src/qml/Service.qml)\n')
        self.write('src/backend/paint.h', 'class LVRS_EXPORT Paint : public QQuickPaintedItem {\n'
                   'QML_NAMED_ELEMENT(Paint)\n};\n')

    def tearDown(self):
        self.temp.cleanup()

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8', newline='\n')

    def audit(self):
        files = sorted(p.relative_to(self.root).as_posix() for p in (self.root / 'src').rglob('*') if p.is_file())
        def git(command, **kwargs):
            if 'rev-parse' in command: return 'fixture-revision\n'
            return ('\0'.join(files) + '\0').encode()
        with patch.object(inventory.subprocess, 'check_output', side_effect=git):
            return inventory.audit(self.root)

    def test_inline_native_and_nonvisual_classification(self):
        snapshot = self.audit()
        declarations = {d['name']: d for d in snapshot['declarations']}
        self.assertEqual(declarations['InnerVisual']['kind'], 'view')
        self.assertTrue(declarations['InnerVisual']['internal'])
        self.assertEqual(declarations['Paint']['kind'], 'view')
        self.assertEqual(declarations['Service']['kind'], 'nonvisual')
        self.assertNotIn('FakeVisual', declarations)

    def test_unknown_base_fails_instead_of_silently_omitting(self):
        self.write('src/qml/Fixture.qml', 'UnresolvedNativeType {}\n')
        with self.assertRaisesRegex(ValueError, 'Unresolved'):
            self.audit()

    def test_orphan_qml_file_fails_build_registration_check(self):
        self.write('src/qml/Unregistered.qml', 'Item {}\n')
        with self.assertRaisesRegex(ValueError, 'build registration'):
            self.audit()

    def test_hash_detects_nonvisual_source_changes(self):
        before = self.audit()
        self.write('src/qml/Service.qml', 'QtObject { property int revision: 2 }\n')
        after = self.audit()
        self.assertNotEqual(before['files'], after['files'])
        self.assertEqual(inventory.words(before), inventory.words(after))

    def test_duplicate_visual_name_retains_origins_but_reserves_once(self):
        self.write('src/qml/Fixture.qml', 'Item {\n component Fixture: Rectangle {}\n}\n')
        snapshot = self.audit()
        self.assertEqual(sum(d['name'] == 'Fixture' for d in snapshot['declarations']), 2)
        self.assertEqual(sum(d['name'] == 'Fixture' for d in inventory.words(snapshot)), 1)

    def test_line_ending_conversion_does_not_change_the_snapshot(self):
        before = self.audit()
        for file in (self.root / 'src').rglob('*'):
            if file.is_file(): file.write_bytes(file.read_bytes().replace(b'\n', b'\r\n'))
        self.assertEqual(before, self.audit())

    def test_unchanged_keyword_outputs_keep_modification_times(self):
        definitions = self.root / 'output/Keywords.def'
        document = self.root / 'output/ReservedWords.md'
        catalog = self.root / 'output/catalog.json'
        def generate(snapshot):
            with patch.object(inventory, 'DEFINITIONS', definitions), \
                    patch.object(inventory, 'DOCUMENT', document), \
                    patch.object(inventory, 'CATALOG', catalog), \
                    patch.object(inventory, 'audit', return_value=snapshot), \
                    patch.object(sys, 'argv', ['keyword-inventory', '--lvrs-root', str(self.root)]), \
                    contextlib.redirect_stdout(io.StringIO()):
                inventory.main()
        before = self.audit()
        generate(before)
        for path in (definitions, document, catalog):
            content = path.read_bytes()
            self.assertNotIn(b'\r\n', content)
            self.assertEqual(content.decode('utf-8'), path.read_text(encoding='utf-8'))
            os.utime(path, (1000, 1000))
        timestamps = {p: p.stat().st_mtime_ns for p in (definitions, document, catalog)}
        generate(before)
        self.assertEqual(timestamps, {p: p.stat().st_mtime_ns for p in timestamps})
        self.write('src/qml/Service.qml', 'QtObject { property int revision: 2 }\n')
        after = self.audit()
        generate(after)
        self.assertEqual(definitions.stat().st_mtime_ns, timestamps[definitions])
        self.assertEqual(document.stat().st_mtime_ns, timestamps[document])
        self.assertNotEqual(catalog.stat().st_mtime_ns, timestamps[catalog])
        self.assertEqual(json.loads(catalog.read_text()), after)


if __name__ == '__main__':
    unittest.main()
