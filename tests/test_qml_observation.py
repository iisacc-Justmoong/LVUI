from pathlib import Path
import hashlib
import importlib.util
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("qml_observation", ROOT / "tools/observe_qml.py")
observer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(observer)


class QmlObservationTests(unittest.TestCase):
    def test_comments_and_literals_do_not_invent_components(self):
        source = '''// FakeView { }
        /* AnotherFake { } */
        import LVRS 1.0 as LV
        Item {
            property string prompt: "HiddenView { } ? onInvented:"
            property string templateText: `MoreHidden { }`
            LV.Label { text: "real" }
        }
        '''
        data = observer.inspect_source(source)
        self.assertEqual([c["name"] for c in data["components"]], ["Item", "LV.Label"])
        self.assertEqual(data["components"][1]["module"], "LVRS")
        self.assertEqual(data["features"]["conditional_operators"], 0)
        self.assertFalse(data["handlers"])

    def test_native_model_and_view_occurrences_remain_distinct_facts(self):
        data = observer.inspect_source('''import Dreamscapes.Storage 1.0
        Item {
            NativeToolState { }
            Repeater { model: vm.rows; LV.Label { text: modelData.name } }
            Loader { active: vm.ready }
        }''')
        self.assertEqual(data["features"]["model_delegates"], 1)
        self.assertEqual(data["features"]["loaders"], 1)
        self.assertIn("NativeToolState", [c["type"] for c in data["components"]])
        self.assertIsNone(data["components"][1]["module"])

    def test_positions_properties_handlers_and_inline_types(self):
        data = observer.inspect_source('''import LVRS 1.0 as LV
component Prompt: LV.TextEditor {
    required property var model
    readonly property alias input: field.editorItem
    property bool wide: width >= 1000
    onTextEdited: value => model.setText(value)
    function focusEditor() { }
}
''')
        self.assertEqual(data["inline_components"], [{"name": "Prompt", "base": "LV.TextEditor", "line": 2}])
        self.assertEqual(data["properties"][0]["line"], 3)
        self.assertEqual(data["features"]["required_properties"], 1)
        self.assertEqual(data["features"]["property_aliases"], 1)
        self.assertEqual(data["handlers"], [{"name": "onTextEdited", "line": 6}])
        self.assertEqual(data["functions"], [{"name": "focusEditor", "line": 7}])
        self.assertEqual(data["lexical_operators"][">="], 1)

    def test_deterministic_inventory_does_not_change_source(self):
        (ROOT / "build").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / "build", prefix="qml-observe-") as tmp:
            root = Path(tmp)
            content = b'import LVRS 1.0 as LV\nLV.Label { text: "hello" }\n'
            path = root / "Main.qml"
            path.write_bytes(content)
            first = observer.observe(root)
            self.assertEqual(first, observer.observe(root))
            self.assertEqual(path.read_bytes(), content)
            self.assertEqual(first["files"][0]["sha256"], hashlib.sha256(content).hexdigest())
            self.assertEqual(first["summary"]["qml_files"], 1)

    def test_commented_import_and_escaped_quotes_are_ignored(self):
        data = observer.inspect_source('''/*
import Incorrect 1.0 as LV
*/
import LVRS 1.0 as LV
LV.Label { text: "escaped \\" Fake { }" }
''')
        self.assertEqual(data["imports"], [{"module": "LVRS", "version": "1.0", "alias": "LV", "line": 4}])
        self.assertEqual([c["name"] for c in data["components"]], ["LV.Label"])


if __name__ == "__main__":
    unittest.main()
