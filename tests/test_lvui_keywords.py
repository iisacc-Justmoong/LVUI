"""Validate the LVUI-owned registry without reading any other project."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('lvui_keywords', ROOT / 'tools/update_keywords.py')
keywords = importlib.util.module_from_spec(spec)
spec.loader.exec_module(keywords)


def registry(entries):
    return json.dumps({'schema': 1, 'entries': entries}, indent=2) + '\n'


class KeywordRegistryTests(unittest.TestCase):
    def test_local_source_and_deterministic_output(self):
        text = keywords.REGISTRY.read_text(encoding='utf-8')
        generated = keywords.render(text)
        self.assertEqual(generated, keywords.OUTPUT.read_text(encoding='utf-8'))
        self.assertNotIn('LVRS/', generated)
        self.assertNotIn('/qml/', generated)
        self.assertIn('"catalog/lvui-keywords.json"', generated)
        self.assertEqual(generated, keywords.render(text))

    def test_rejects_duplicates_unsorted_names_and_unknown_kinds(self):
        view = {'name': 'A', 'kind': 'view', 'internal': False}
        for entries in ([view, view], [dict(view, name='B'), view], [dict(view, kind='unknown')]):
            with self.assertRaises(ValueError):
                keywords.render(registry(entries))

    def test_rejects_wrong_types_and_malformed_identifiers(self):
        for entry in ({'name': 'bad-name', 'kind': 'view', 'internal': False},
                      {'name': 'A', 'kind': 'view', 'internal': 'false'},
                      {'name': 'A', 'kind': 'view', 'internal': False, 'source': 'other'}):
            with self.assertRaises(ValueError):
                keywords.render(registry([entry]))

    def test_rejects_an_incomplete_language_core(self):
        with self.assertRaisesRegex(ValueError, 'language core'):
            keywords.render(registry([]))

    def test_legacy_generator_cannot_overwrite_the_active_registry(self):
        before = keywords.OUTPUT.read_bytes()
        result = subprocess.run([sys.executable, '-B', str(ROOT / 'tools/update_lvrs_keywords.py'), '--snapshot-only'],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('cannot update LVUI keywords', result.stderr)
        self.assertEqual(keywords.OUTPUT.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
