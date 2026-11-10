import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("figma_defaults", ROOT / "tools/update_figma_defaults.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class FigmaComponentCatalogTests(unittest.TestCase):
    def setUp(self):
        self.snapshot = json.loads(generator.SOURCE.read_text(encoding="utf-8"))

    def test_snapshot_covers_all_observed_ui_pages_and_components(self):
        self.assertEqual(len(self.snapshot["pages"]), 23)
        components = [c for page in self.snapshot["pages"] for c in page["components"]]
        self.assertEqual(len(components), 119)
        self.assertEqual(len({c["id"] for c in components}), len(components))
        self.assertTrue(self.snapshot["source"].startswith("https://www.figma.com/design/0GkItQYSNIR0lZ3iJhfJzc/"))

    def test_generated_defaults_match_the_snapshot(self):
        actual = generator.render(self.snapshot)
        self.assertEqual(actual, generator.OUTPUT.read_text(encoding="utf-8"))
        self.assertEqual(actual, generator.render(self.snapshot))
        for expected in ('LVUI_FIGMA_DEFAULT("44:599", "Kind", "default")',
                         'LVUI_FIGMA_DEFAULT("44:724", "State", "On")',
                         'LVUI_FIGMA_DEFAULT("879:6541", "Size", "Mini")',
                         'LVUI_FIGMA_DEFAULT("1243:1226", "Icon", "projectStructure")'):
            self.assertIn(expected, actual)

    def test_rejects_an_invalid_variant_default(self):
        component = next(c for p in self.snapshot["pages"] for c in p["components"] if c["id"] == "44:599")
        component["properties"]["Kind"]["defaultValue"] = "not-a-variant"
        with self.assertRaisesRegex(ValueError, "not a Figma variant"):
            generator.render(self.snapshot)


if __name__ == "__main__":
    unittest.main()
