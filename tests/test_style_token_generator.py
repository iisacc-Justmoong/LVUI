"""The captured Figma catalog must remain complete and reproducible offline."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("style_token_generator", ROOT / "tools/update_style_tokens.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class StyleTokenGeneratorTests(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((ROOT / "catalog/lvrs-style-tokens.json").read_text(encoding="utf-8"))

    def test_complete_frozen_catalog(self):
        self.assertEqual(self.data["source"]["fileKey"], "0GkItQYSNIR0lZ3iJhfJzc")
        self.assertEqual(len(self.data["variables"]), 208)
        self.assertEqual([c["count"] for c in self.data["collections"]], [115, 20, 5, 20, 48])
        self.assertEqual(generator.generate(self.data), generator.OUTPUT.read_text(encoding="utf-8"))
        self.assertEqual(generator.generate(self.data), generator.generate(copy.deepcopy(self.data)))

    def test_truncation_and_duplicate_identity_are_rejected(self):
        self.data["variables"].pop()
        with self.assertRaises(ValueError):
            generator.generate(self.data)

    def test_aliases_are_preserved_and_checked(self):
        aliases = [v for v in self.data["variables"] if any(isinstance(x, dict) and x.get("type") == "VARIABLE_ALIAS" for x in v["valuesByMode"].values())]
        self.assertEqual(len(aliases), 6)
        alias = aliases[0]
        mode = next(iter(alias["valuesByMode"]))
        for target in ["missing", alias["id"]]:
            alias["valuesByMode"][mode]["id"] = target
            with self.assertRaises(ValueError):
                generator.generate(self.data)

    def test_incomplete_modes_are_rejected(self):
        self.data["variables"][0]["valuesByMode"].clear()
        with self.assertRaises(ValueError):
            generator.generate(self.data)

    def test_css_collision_and_invalid_values_are_rejected(self):
        first, second = self.data["variables"][:2]
        second["codeSyntax"]["WEB"] = first["codeSyntax"]["WEB"]
        with self.assertRaises(ValueError):
            generator.generate(self.data)
        second["codeSyntax"]["WEB"] = "var(--lvrs-test-valid)"
        color = next(v for v in self.data["variables"] if v["type"] == "COLOR" and "r" in next(iter(v["valuesByMode"].values())))
        value = next(iter(color["valuesByMode"].values()))
        value["r"] = float("nan")
        with self.assertRaises(ValueError):
            generator.generate(self.data)

    def test_string_and_boolean_types_cannot_be_coerced(self):
        value = next(v for v in self.data["variables"] if v["type"] == "STRING")
        value["valuesByMode"][next(iter(value["valuesByMode"]))] = False
        with self.assertRaises(ValueError):
            generator.generate(self.data)

    def test_utf8_literals_do_not_emit_surrogate_escapes(self):
        literal = generator.literal('색상 🙂 "\\')
        self.assertNotIn('\\ud83d', literal)
        self.assertIn('\\360\\237\\231\\202', literal)
        self.assertIn('\\"', literal)

    def test_alias_graph_respects_collection_mode_selection(self):
        collection = next(c for c in self.data['collections'] if c['name'] == 'Theme Typography')
        variables = [v for v in self.data['variables'] if v['collectionId'] == collection['id']]
        dark = collection['defaultModeId']
        light = 'test-light'
        collection['modes'].append({'modeId': light, 'name': 'Light'})
        for variable in variables:
            variable['valuesByMode'][light] = copy.deepcopy(variable['valuesByMode'][dark])
        first, second = [v for v in variables if v['type'] == 'FLOAT'][:2]
        # Opposite aliases in mutually exclusive modes do not form a cycle.
        first['valuesByMode'][dark] = {'type': 'VARIABLE_ALIAS', 'id': second['id']}
        second['valuesByMode'][light] = {'type': 'VARIABLE_ALIAS', 'id': first['id']}
        self.assertIn('test-light', generator.generate(self.data))
        second['valuesByMode'][dark] = {'type': 'VARIABLE_ALIAS', 'id': first['id']}
        with self.assertRaises(ValueError):
            generator.generate(self.data)

    def test_typed_members_cover_every_color_and_metric(self):
        collections = {c['id']: c for c in self.data['collections']}
        members = [generator.selector(v, collections[v['collectionId']]) for v in self.data['variables']]
        self.assertEqual(sum(x.startswith('Color.') for x in members), 138)
        self.assertEqual(sum(x.startswith('Metric.') for x in members), 67)
        self.assertEqual(len([x for x in members if x]), len(set(members) - {''}))
        self.assertIn('Color.primary', members)
        self.assertIn('Color.material.surface.dense.edge', members)
        self.assertIn('Metric.space8', members)
        self.assertIn('Metric.stroke1_5', members)
        self.assertEqual(generator.selector_document(self.data), (ROOT/'docs/StyleTokenMembers.md').read_text())

    def test_normalized_member_collisions_are_rejected(self):
        first = next(v for v in self.data['variables'] if v['name'] == 'Space/8')
        second = next(v for v in self.data['variables'] if v['name'] == 'Space/12')
        second['name'] = first['name']
        with self.assertRaisesRegex(ValueError, 'colliding style selector'):
            generator.generate(self.data)


if __name__ == "__main__":
    unittest.main()
