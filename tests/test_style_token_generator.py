"""The captured Figma catalog must remain complete and reproducible offline."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("style_token_generator", ROOT / "tools/update_style_tokens.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
icon_spec = importlib.util.spec_from_file_location("icon_generator", ROOT / "tools/embed_icons.py")
icons = importlib.util.module_from_spec(icon_spec)
icon_spec.loader.exec_module(icons)


class StyleTokenGeneratorTests(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((ROOT / "catalog/lvui-style-tokens.json").read_text(encoding="utf-8"))

    def test_button_motion_snapshot_is_validated_and_generated(self):
        snapshot = json.loads((ROOT / "catalog/lvui-button-motion.json").read_text(encoding="utf-8"))
        header = generator.button_motion_defaults(snapshot)
        self.assertEqual(header, generator.BUTTON_MOTION_DEFAULTS.read_text(encoding="utf-8"))
        self.assertIn("hover{100}", header)
        changed = copy.deepcopy(snapshot)
        changed['hoverMilliseconds'] = 140
        self.assertIn("hover{140}", generator.button_motion_defaults(changed))
        for field, value in [('hoverMilliseconds', True), ('pressMilliseconds', 0),
                             ('releaseMilliseconds', -1), ('postReleaseMilliseconds', 10001),
                             ('easing', 'unknown'), ('enabledVariantCount', 51),
                             ('sources', [])]:
            with self.subTest(field=field):
                changed = copy.deepcopy(snapshot)
                changed[field] = value
                with self.assertRaises(ValueError):
                    generator.button_motion_defaults(changed)

    def test_material_radial_snapshot_rejects_invalid_geometry_and_stops(self):
        snapshot = json.loads((ROOT / "catalog/lvui-material-radial.json").read_text(encoding="utf-8"))
        self.assertEqual(generator.material_radial_defaults(snapshot),
                         generator.MATERIAL_RADIAL_DEFAULTS.read_text(encoding="utf-8"))
        for mutate in (
            lambda value: value['stops'][1].update(position=.8),
            lambda value: value['stops'][1]['color'].__setitem__(0, 2),
            lambda value: value['fields'][0]['radius'].__setitem__(0, 0),
            lambda value: value['fields'][0].update(strength='unknown'),
        ):
            with self.subTest(mutate=mutate):
                changed = copy.deepcopy(snapshot)
                mutate(changed)
                with self.assertRaises(ValueError):
                    generator.material_radial_defaults(changed)

    def test_complete_frozen_catalog(self):
        self.assertEqual(self.data["source"]["fileKey"], "8nSrpI0BbsZlXQiotbClHD")
        self.assertEqual(len(self.data["variables"]), 242)
        self.assertEqual([c["count"] for c in self.data["collections"]], [145, 20, 5, 20, 52])
        self.assertTrue(all(v['id'].startswith('VariableID:') and v['cssName'].startswith('--lvui-')
                            for v in self.data['variables']))
        self.assertEqual(sum(len(v.get('legacyAliases', [])) == 3 for v in self.data['variables']), 237)
        self.assertEqual(sum(not v.get('legacyAliases') for v in self.data['variables']), 5)
        self.assertEqual(generator.generate(self.data), generator.OUTPUT.read_text(encoding="utf-8"))
        self.assertEqual(generator.compatibility(self.data), generator.ALIASES.read_text(encoding="utf-8"))
        self.assertEqual(generator.generate(self.data), generator.generate(copy.deepcopy(self.data)))

    def test_truncation_and_duplicate_identity_are_rejected(self):
        self.data["variables"].pop()
        with self.assertRaises(ValueError):
            generator.generate(self.data)

    def test_figma_syntax_and_legacy_identity_collisions_are_rejected(self):
        first, second = self.data['variables'][:2]
        second['legacyAliases'].append(first['cssName'])
        with self.assertRaisesRegex(ValueError, 'Colliding token identity'):
            generator.generate(self.data)
        second['legacyAliases'].pop()
        first['codeSyntax']['WEB'] = 'Metric.notTheActualMember'
        with self.assertRaisesRegex(ValueError, 'Figma code syntax'):
            generator.generate(self.data)

    def test_renamed_figma_library_identities_are_unambiguous(self):
        source = self.data['canonicalSource']
        self.assertEqual(source['fileKey'], '0GkItQYSNIR0lZ3iJhfJzc')
        self.assertEqual(source['matchedExistingVariables'], 233)
        self.assertEqual(len(source['retiredVariableIds']), 4)
        expected = {
            'Icon/search': ('VariableID:1814:247', 'cfffd9e1c8da3bd933aa6b19de0e5866352b4352'),
            'TextField/paddingHorizontal': ('VariableID:1814:248', 'd1f555fd332365eefa4adb5e98f79e2c84fb4627'),
            'TextField/paddingVertical': ('VariableID:1814:249', 'f737efc560f38281163aaa5f90d49dd830d53c48'),
            'TextField/iconSize': ('VariableID:1814:250', 'ca4a7d732e41cdf55d1a1d3ab3daf444d67e07fd'),
            'TextField/borderWidth': ('VariableID:1814:251', 'ca4f47d8c2c877df794f08e5643bb11d14d6a200'),
        }
        variables = {v['name']: v for v in self.data['variables']}
        for name, identities in expected.items():
            with self.subTest(name=name):
                self.assertEqual(variables[name]['sourceAliases'], list(identities))
                for identity in identities:
                    self.assertIn(identity, generator.compatibility(self.data))
        first, second = self.data['variables'][:2]
        second['sourceAliases'] = [first['id']]
        with self.assertRaisesRegex(ValueError, 'Colliding token identity'):
            generator.generate(self.data)

    def test_previous_color_tracks_the_current_figma_target(self):
        by_name = {v['name']: v for v in self.data['variables']}
        previous = by_name['Previous color']
        self.assertEqual(next(iter(previous['valuesByMode'].values())), {
            'type': 'VARIABLE_ALIAS', 'id': by_name['Accent/Blue/Default']['id'],
        })

    def test_source_aliases_reject_invalid_identity_data(self):
        variable = self.data['variables'][0]
        for value in ([''], ['duplicate', 'duplicate'], ['valid', None], 'not-a-list'):
            variable['sourceAliases'] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                generator.generate(self.data)

    def test_builtin_compiler_uses_lvui_token_names(self):
        for path in (ROOT / 'src/Compiler/Parser.cpp', ROOT / 'src/Compiler/Compiler.cpp'):
            with self.subTest(path=path.name):
                self.assertFalse('--lvrs-' in path.read_text(encoding='utf-8'), 'Compiler uses a legacy token name')

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
        second["cssName"] = first["cssName"]
        with self.assertRaises(ValueError):
            generator.generate(self.data)
        second["cssName"] = "--lvui-test-valid"
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
        self.assertEqual(sum(x.startswith('Color.') for x in members), 168)
        self.assertEqual(sum(x.startswith('Metric.') for x in members), 71)
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
        second['codeSyntax']['WEB'] = first['codeSyntax']['WEB']
        with self.assertRaisesRegex(ValueError, 'colliding style selector'):
            generator.generate(self.data)

    def test_current_figma_color_readback_and_defaults_are_reproducible(self):
        readback = self.data['colorReadback']
        self.assertEqual(readback['capturedOn'], '2026-11-02')
        self.assertEqual(readback['fileKey'], '8nSrpI0BbsZlXQiotbClHD')
        self.assertEqual(len(readback['variableIds']), 145)
        self.assertIn('VariableID:2:186', readback['variableIds'])
        historical = self.data['provenance']['colorReadback']
        self.assertEqual(historical['capturedOn'], '2026-10-31')
        self.assertEqual(len(historical['variableIds']), 141)
        self.assertEqual(historical['additionalReadbacks'], [{
            'provider': 'Figma', 'fileKey': '0GkItQYSNIR0lZ3iJhfJzc',
            'capturedOn': '2026-11-02',
            'variableIds': ['VariableID:44:458', 'VariableID:44:462'],
        }])
        header = generator.HEADER.read_text(encoding='utf-8')
        self.assertEqual(generator.default_color_header(self.data, header), header)
        colors = next(c['id'] for c in self.data['collections'] if c['name'] == 'Theme Colors')
        by_name = {v['name']: v for v in self.data['variables'] if v['collectionId'] == colors}
        darker = [v for v in by_name.values() if v['name'].startswith('Icon/Accent/')]
        self.assertEqual(len(darker), 29)
        for icon in darker:
            base = by_name[icon['name'].removeprefix('Icon/').removesuffix('/Darker')]
            iv, bv = next(iter(icon['valuesByMode'].values())), next(iter(base['valuesByMode'].values()))
            # Figma owns the exact channel values; only the darker role is a
            # design contract, not a fixed RGB scaling formula.
            brightness = lambda color: sum(color[k] * weight for k, weight in zip('rgb', (.2126, .7152, .0722)))
            self.assertLess(brightness(iv), brightness(bv), icon['name'])

    def test_additional_color_readbacks_extend_native_defaults(self):
        ids = [v['id'] for v in self.data['variables'] if v['name'] in ('Text/titleHeaderColor', 'Text/disabledColor')]
        self.data['colorReadback']['variableIds'] = [v for v in self.data['colorReadback']['variableIds'] if v not in ids]
        self.data['colorReadback']['additionalReadbacks'] = [{
            'provider': 'Figma', 'fileKey': '8nSrpI0BbsZlXQiotbClHD',
            'capturedOn': '2026-11-02',
            'variableIds': ids,
        }]
        header = generator.default_color_header(self.data, generator.HEADER.read_text(encoding='utf-8'))
        self.assertIn('inline constexpr StyleColor text_title_header_color{', header)
        self.assertIn('inline constexpr StyleColor text_disabled_color{', header)
        self.data['colorReadback']['additionalReadbacks'][0]['variableIds'].append('missing')
        with self.assertRaises(ValueError):
            generator.default_color_header(self.data, header)

    def test_slider_symbol_material_readback_is_generated(self):
        captures = self.data['colorReadback'].get('additionalReadbacks', [])
        self.assertTrue(any(c.get('fileKey') == '0GkItQYSNIR0lZ3iJhfJzc' and
                            'VariableID:2:41' in c.get('variableIds', []) for c in captures))
        header = generator.default_color_header(self.data, generator.HEADER.read_text(encoding='utf-8'))
        self.assertIn('inline constexpr StyleColor material_alert_action_border{', header)
        svg = ET.fromstring((ROOT / 'resources/icons/sliderSymbolSun.svg').read_text(encoding='utf-8'))
        paths = svg.findall('{http://www.w3.org/2000/svg}path')
        self.assertEqual(len(paths), 8)
        for path in paths:
            self.assertEqual(path.get('fill'), '#596168')
            self.assertEqual(path.get('data-lvui-fill-token'), 'Material/Alert/actionBorder')
            self.assertNotIn('fill-opacity', path.attrib)
        alias = ET.fromstring((ROOT / 'resources/authored-icons/sun.svg').read_text(encoding='utf-8'))
        paths = alias.findall('{http://www.w3.org/2000/svg}path')
        self.assertEqual(len(paths), 8)
        for path in paths:
            self.assertEqual(path.get('fill'), '#596168')
            self.assertNotIn('fill-opacity', path.attrib)

    def test_icon_binding_follows_token_changes_and_rejects_invalid_bindings(self):
        source = '<svg xmlns="http://www.w3.org/2000/svg"><path d="M1 2" stroke="#FFFFFF" data-lvui-stroke-token="Theme Colors/Accent/Blue/Hover"/></svg>'
        resolved = icons.resolve_svg_tokens(source, self.data)
        self.assertIn('stroke="#2373CD"', resolved)
        self.assertIn('d="M1 2"', resolved)
        changed = copy.deepcopy(self.data)
        blue = next(v for v in changed['variables'] if v['name'] == 'Accent/Blue/Hover')
        value = next(iter(blue['valuesByMode'].values()))
        value.update(r=1.0, g=0.0, b=0.0)
        self.assertIn('stroke="#FF0000"', icons.resolve_svg_tokens(source, changed))
        for invalid in (source.replace('Blue/Hover', 'Blue/Unknown'), source.replace('stroke="#FFFFFF"', '')):
            with self.assertRaises(ValueError):
                icons.resolve_svg_tokens(invalid, self.data)

    def test_search_field_tokens_match_the_independent_figma_library(self):
        expected = {
            'Icon/search': ('VariableID:33:7', {'r': 206 / 255, 'g': 208 / 255, 'b': 214 / 255, 'a': 1}),
            'TextField/paddingHorizontal': ('VariableID:34:338', 7),
            'TextField/paddingVertical': ('VariableID:34:339', 4.5),
            'TextField/iconSize': ('VariableID:34:340', 12),
            'TextField/borderWidth': ('VariableID:34:341', .5),
        }
        variables = {v['name']: v for v in self.data['variables']}
        for name, (identifier, expected_value) in expected.items():
            with self.subTest(name=name):
                variable = variables[name]
                self.assertEqual(variable['id'], identifier)
                self.assertFalse(variable.get('legacyAliases'))
                value = next(iter(variable['valuesByMode'].values()))
                if isinstance(expected_value, dict):
                    for channel, component in expected_value.items():
                        self.assertAlmostEqual(value[channel], component, places=7)
                else:
                    self.assertEqual(value, expected_value)

    def test_search_field_defaults_are_compiled_from_lvui_tokens(self):
        header = generator.search_field_defaults(self.data)
        self.assertEqual(header, generator.SEARCH_DEFAULTS.read_text(encoding='utf-8'))
        self.assertIn('inline constexpr double icon_size = 12.0;', header)
        self.assertIn('inline constexpr double horizontal_padding = 7.0;', header)
        self.assertIn('inline constexpr StyleColor disabled_text{', header)
        self.assertIn('inline constexpr StyleColor disabled_background{', header)
        changed = copy.deepcopy(self.data)
        value = next(v for v in changed['variables'] if v['name'] == 'TextField/iconSize')
        value['valuesByMode']['2:4'] = 16
        self.assertIn('inline constexpr double icon_size = 16.0;', generator.search_field_defaults(changed))
        value['valuesByMode']['2:4'] = {'type': 'VARIABLE_ALIAS', 'id': 'missing'}
        with self.assertRaises(ValueError):
            generator.search_field_defaults(changed)

    def test_search_field_uses_original_enabled_binding_and_disabled_material(self):
        header = generator.search_field_defaults(self.data)
        self.assertIn('inline constexpr StyleColor background{0.14418363571166992, 0.1475532352924347, '
                      '0.1475532352924347, 0.6399999856948853};', header)
        material = json.loads(generator.SEARCH_MATERIAL_SOURCE.read_text(encoding='utf-8'))
        changed = copy.deepcopy(self.data)
        surface = next(v for v in changed['variables'] if v['name'] == 'Panel/surfaceSolid')
        surface['valuesByMode']['2:0']['r'] = .25
        self.assertIn('inline constexpr StyleColor background{0.25,', generator.search_field_defaults(changed, material))
        for key, value in (('backgroundVariableId', 'missing'), ('backgroundOpacity', 1.5)):
            invalid = copy.deepcopy(material)
            invalid[key] = value
            with self.assertRaises(ValueError):
                generator.search_field_defaults(self.data, invalid)
        disabled = json.loads(generator.SEARCH_DISABLED_SOURCE.read_text(encoding='utf-8'))
        self.assertIn('inline constexpr SearchGradientStop disabled_fill_gradient[]',
                      generator.search_material_defaults(material, disabled))
        disabled['effects'] = [{'type': 'BACKGROUND_BLUR', 'radius': 8}]
        with self.assertRaises(ValueError):
            generator.search_material_defaults(material, disabled)

    def test_cylinder_radius_comes_from_the_figma_component(self):
        cylinder = json.loads(generator.SEARCH_CYLINDER_SOURCE.read_text(encoding='utf-8'))
        material = json.loads(generator.SEARCH_MATERIAL_SOURCE.read_text(encoding='utf-8'))
        header = generator.search_field_defaults(self.data, material, cylinder)
        self.assertIn('inline constexpr double cylinder_radius = 99.0;', header)
        cylinder['cornerRadius'] = -1
        with self.assertRaises(ValueError):
            generator.search_field_defaults(self.data, material, cylinder)

    def test_inline_material_comes_from_its_figma_states(self):
        inline = json.loads(generator.SEARCH_INLINE_SOURCE.read_text(encoding='utf-8'))
        material = json.loads(generator.SEARCH_MATERIAL_SOURCE.read_text(encoding='utf-8'))
        disabled = json.loads(generator.SEARCH_DISABLED_SOURCE.read_text(encoding='utf-8'))
        header = generator.search_material_defaults(material, disabled, inline)
        self.assertIn('inline constexpr double inline_blur_radius = 6.0;', header)
        self.assertIn('inline constexpr SearchGradientStop inline_fill_gradient[]', header)
        self.assertIn('inline constexpr SearchShadow inline_inner_shadows[]', header)
        changed = copy.deepcopy(inline)
        changed['disabled']['effects'] = [{'type': 'BACKGROUND_BLUR', 'radius': 6}]
        with self.assertRaises(ValueError):
            generator.search_material_defaults(material, disabled, changed)

    def test_icon_button_defaults_are_compiled_from_figma_geometry(self):
        snapshot = json.loads(generator.ICON_BUTTON_SOURCE.read_text(encoding='utf-8'))
        header = generator.icon_button_defaults(snapshot)
        self.assertEqual(header, generator.ICON_BUTTON_DEFAULTS.read_text(encoding='utf-8'))
        for expected in ('double extent = 22.0;', 'double icon_size = 18.0;',
                         'double horizontal_padding = 2.0;', 'double corner_radius = 8.0;'):
            self.assertIn(expected, header)
        changed = copy.deepcopy(snapshot)
        changed['iconWidth'] = 17
        with self.assertRaises(ValueError):
            generator.icon_button_defaults(changed)
        changed = copy.deepcopy(snapshot)
        changed.update(width=24, height=24, iconWidth=20, iconHeight=20)
        result = generator.icon_button_defaults(changed)
        self.assertIn('double extent = 24.0;', result)
        self.assertIn('double icon_size = 20.0;', result)
        for field, value in (('verifiedVariantCount', 0), ('paddingLeft', True),
                             ('cornerRadius', 12)):
            changed = copy.deepcopy(snapshot)
            changed[field] = value
            with self.assertRaises(ValueError):
                generator.icon_button_defaults(changed)

    def test_segmented_defaults_are_compiled_from_figma_variants(self):
        snapshot = json.loads(generator.SEGMENTED_SOURCE.read_text(encoding='utf-8'))
        self.assertEqual(snapshot['borderVariableUsage'], 'explicit-override-fallback')
        header = generator.segmented_defaults(self.data, snapshot)
        self.assertEqual(header, generator.SEGMENTED_DEFAULTS.read_text(encoding='utf-8'))
        for expected in ('double label_vertical_padding = 0.0;', 'double icon_vertical_padding = 0.0;',
                         'double spacing = 2.0;', 'double border_width = 0.0;', 'double corner_radius = 8.0;'):
            self.assertIn(expected, header)
        changed = copy.deepcopy(snapshot)
        changed['icon']['verticalPadding'] = 5
        self.assertIn('double icon_vertical_padding = 5.0;', generator.segmented_defaults(self.data, changed))
        for field, value in (('schema', True), ('backgroundVariableKey', 'wrong'),
                             ('backgroundVariableKey', '1f0635c7724509149a06c3e3b411923801f179ba'),
                             ('cornerRadiusVariableKey', 'wrong'), ('cornerRadius', 12),
                             ('borderVariableKey', 'wrong'), ('spacing', -1)):
            changed = copy.deepcopy(snapshot)
            changed[field] = value
            with self.assertRaises(ValueError):
                generator.segmented_defaults(self.data, changed)

    def test_design_icon_defaults_are_compiled_from_figma_geometry(self):
        snapshot = json.loads(generator.DESIGN_ICON_SOURCE.read_text(encoding='utf-8'))
        header = generator.design_icon_defaults(snapshot)
        self.assertEqual(header, generator.DESIGN_ICON_DEFAULTS.read_text(encoding='utf-8'))
        self.assertIn('double extent = 18.0;', header)
        changed = copy.deepcopy(snapshot)
        changed.update(width=24, height=24)
        self.assertIn('double extent = 24.0;', generator.design_icon_defaults(changed))
        for field, value in (('schema', True), ('width', True), ('height', 17), ('componentKey', ''), ('width', 0)):
            changed = copy.deepcopy(snapshot)
            changed[field] = value
            with self.assertRaises(ValueError):
                generator.design_icon_defaults(changed)

    def test_slider_center_is_compiled_from_the_figma_snapshot(self):
        snapshot = json.loads(generator.SLIDER_CENTER_SOURCE.read_text(encoding='utf-8'))
        header = generator.slider_center_defaults(self.data, snapshot)
        self.assertEqual(header, generator.SLIDER_CENTER.read_text(encoding='utf-8'))
        self.assertIn('inline constexpr double width = 2.0;', header)
        self.assertIn('inline constexpr double overhang = 2.0;', header)
        self.assertIn('inline constexpr double radius = 1.0;', header)
        self.assertIn('inline constexpr StyleColor color{1.0, 1.0, 1.0, 0.5};', header)
        self.assertEqual(len(snapshot['variants']), 16)
        for mutation in ('missing', 'bad_width', 'bad_radius', 'bad_color', 'bad_alias', 'bad_overhang', 'duplicate', 'boolean_opacity'):
            changed = json.loads(json.dumps(snapshot))
            if mutation == 'missing': changed['variants'].pop()
            elif mutation == 'bad_width': changed['variants'][0]['markWidth'] = 3
            elif mutation == 'bad_radius': changed['variants'][0]['markCornerRadius'] = 0
            elif mutation == 'bad_color': changed['fillVariableId'] = 'VariableID:missing'
            elif mutation == 'bad_alias': changed['sourceFillVariableId'] = 'VariableID:missing'
            elif mutation == 'bad_overhang': changed['variants'][0]['markOverhang'] = 9
            elif mutation == 'duplicate': changed['variants'][1]['node'] = changed['variants'][0]['node']
            else: changed['variants'][0]['opacity'] = True
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                generator.slider_center_defaults(self.data, changed)

    def test_filled_slider_sizes_are_compiled_from_the_figma_snapshot(self):
        snapshot = json.loads((generator.ROOT / 'catalog/lvui-slider-filled.json').read_text(encoding='utf-8'))
        header = generator.slider_filled_defaults(self.data, snapshot)
        self.assertEqual(header, (generator.ROOT / 'src/Style/SliderFilled.generated.h').read_text(encoding='utf-8'))
        for expected in ('SizePreset{22.0, 12.0, 8.0, 6.0, false}',
                         'SizePreset{24.0, 16.0, 8.0, 8.0, false}',
                         'SizePreset{32.0, 28.0, 20.0, 14.0, true}',
                         'SizePreset{44.0, 44.0, 36.0, 22.0, true}'):
            self.assertIn(expected, header)
        self.assertIn('inline constexpr double blur_radius = 6.0;', header)
        self.assertIn('inline constexpr double highlight_offset_y = -0.5;', header)
        for mutation in ('missing', 'duplicate', 'bad_inset', 'bad_thumb', 'bad_symbol', 'bad_opacity', 'bad_token', 'bad_effect'):
            changed = copy.deepcopy(snapshot)
            if mutation == 'missing': changed['variants'].pop()
            elif mutation == 'duplicate': changed['variants'][1]['node'] = changed['variants'][0]['node']
            elif mutation == 'bad_inset': changed['variants'][0]['rangeInset'] += 1
            elif mutation == 'bad_thumb': changed['variants'][1]['thumbDiameter'] += 1
            elif mutation == 'bad_symbol': changed['variants'][0]['symbolVisible'] = 'false'
            elif mutation == 'bad_opacity': changed['variants'][0]['opacity'] = True
            elif mutation == 'bad_token': changed['sourceVariables'][0]['id'] = 'VariableID:missing'
            else: changed['trackEffects'][0]['radius'] = -1
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                generator.slider_filled_defaults(self.data, changed)

    def test_labeled_and_segmented_slider_defaults_match_the_figma_matrix(self):
        snapshot = json.loads((generator.ROOT / 'catalog/lvui-slider-labeled-segmented.json').read_text(encoding='utf-8'))
        filled = json.loads((generator.ROOT / 'catalog/lvui-slider-filled.json').read_text(encoding='utf-8'))
        header = generator.slider_segmented_defaults(self.data, snapshot, filled)
        self.assertEqual(header, (generator.ROOT / 'src/Style/SliderSegmented.generated.h').read_text(encoding='utf-8'))
        self.assertIn('inline constexpr std::size_t default_segment_count = 5;', header)
        self.assertIn('inline constexpr std::array<double, 4> tick_diameters{3.0, 4.0, 4.0, 4.0};', header)
        self.assertIn('inline constexpr double maximum_tick_diameter = 4.0;', header)
        self.assertIn('inline constexpr double tick_track_divisor = 4.0;', header)
        for mutation in ('missing', 'duplicate', 'geometry', 'labels', 'symbol', 'thumb', 'count', 'tick', 'positions', 'opacity', 'color'):
            changed = copy.deepcopy(snapshot)
            row = changed['sets'][1]['variants'][0]
            if mutation == 'missing': changed['sets'][0]['variants'].pop()
            elif mutation == 'duplicate': changed['sets'][1]['variants'][1]['node'] = row['node']
            elif mutation == 'geometry': row['trackHeight'] += 1
            elif mutation == 'labels': row['controlX'] += 1
            elif mutation == 'symbol': row['symbolVisible'] = True
            elif mutation == 'thumb': row['thumbDiameter'] = None
            elif mutation == 'count': row['tickCount'] = 6
            elif mutation == 'tick': row['tickDiameter'] = 7
            elif mutation == 'positions': row['tickPositions'][1] = .3
            elif mutation == 'opacity': row['opacity'] = True
            else: row['tickVariable'] = 'VariableID:missing'
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                generator.slider_segmented_defaults(self.data, changed, filled)

    def test_slider_ticks_are_compiled_from_the_figma_snapshot(self):
        snapshot = json.loads(generator.SLIDER_TICKS_SOURCE.read_text(encoding='utf-8'))
        header = generator.slider_tick_defaults(self.data, snapshot)
        self.assertEqual(header, generator.SLIDER_TICKS.read_text(encoding='utf-8'))
        self.assertIn('inline constexpr double gap = 3.0;', header)
        self.assertIn('inline constexpr double size = 2.0;', header)
        self.assertIn('inline constexpr std::size_t count = 11;', header)
        self.assertIn('inline constexpr StyleColor color{1.0, 1.0, 1.0, 0.5};', header)
        changed = copy.deepcopy(snapshot)
        changed['gapBelowTrack'] = 4
        self.assertIn('inline constexpr double gap = 4.0;', generator.slider_tick_defaults(self.data, changed))
        changed['tickSize'] = -1
        with self.assertRaises(ValueError):
            generator.slider_tick_defaults(self.data, changed)
        changed = copy.deepcopy(snapshot)
        changed['fillVariableId'] = 'missing'
        with self.assertRaises(ValueError):
            generator.slider_tick_defaults(self.data, changed)

    def test_slider_symbol_geometry_is_compiled_from_figma(self):
        snapshot = json.loads(generator.SLIDER_SYMBOL_SOURCE.read_text(encoding='utf-8'))
        header = generator.slider_symbol_defaults(snapshot)
        self.assertEqual(header, generator.SLIDER_SYMBOL.read_text(encoding='utf-8'))
        self.assertIn('inline constexpr double size = 18.0;', header)
        self.assertIn('inline constexpr char icon[] = "sliderSymbolSun";', header)
        self.assertIn('inline constexpr char legacy_icon[] = "sun";', header)
        changed = copy.deepcopy(snapshot)
        changed['symbolSize'] = 16
        self.assertIn('inline constexpr double size = 16.0;', generator.slider_symbol_defaults(changed))
        changed['defaultIcon'] = ''
        with self.assertRaises(ValueError):
            generator.slider_symbol_defaults(changed)
        changed = copy.deepcopy(snapshot)
        changed['legacyIcon'] = ''
        with self.assertRaises(ValueError):
            generator.slider_symbol_defaults(changed)

    def test_slider_endpoints_compile_figma_tokens_and_legacy_minimum_width(self):
        snapshot = json.loads(generator.SLIDER_ENDPOINT_SOURCE.read_text(encoding='utf-8'))
        header = generator.slider_endpoint_defaults(self.data, snapshot)
        self.assertEqual(header, generator.SLIDER_ENDPOINTS.read_text(encoding='utf-8'))
        for expected in ('outer_gap = 8.0', 'inner_gap = 4.0', 'minimum_control_width = 40.0',
                         'font_size = 13.0', 'line_height = 13.0', 'font_weight = 500',
                         'minimum_label[] = "0%"', 'maximum_label[] = "100%"'):
            self.assertIn(expected, header)
        changed = copy.deepcopy(self.data)
        gap = next(v for v in changed['variables'] if v['name'] == 'Space/8')
        gap['valuesByMode']['2:4'] = 10
        self.assertIn('outer_gap = 10.0', generator.slider_endpoint_defaults(changed, snapshot))
        snapshot['minimumControlWidth'] = -1
        with self.assertRaises(ValueError):
            generator.slider_endpoint_defaults(self.data, snapshot)

    def test_search_material_is_compiled_from_the_figma_snapshot(self):
        data = json.loads(generator.SEARCH_MATERIAL_SOURCE.read_text(encoding='utf-8'))
        header = generator.search_material_defaults(data)
        self.assertEqual(header, generator.SEARCH_MATERIAL.read_text(encoding='utf-8'))
        self.assertIn('inline constexpr double blur_radius = 8.0;', header)
        self.assertIn('inline constexpr SearchGradientStop fill_gradient[]', header)
        self.assertIn('inline constexpr SearchShadow inner_shadows[]', header)
        changed = copy.deepcopy(data)
        changed['effects'][0]['radius'] = 12
        self.assertIn('inline constexpr double blur_radius = 12.0;', generator.search_material_defaults(changed))
        changed['fillGradient']['gradientStops'][1]['position'] = -1
        with self.assertRaises(ValueError):
            generator.search_material_defaults(changed)
        for kind, field, value in (('fillGradient', 'visible', False),
                                   ('fillGradient', 'opacity', .5),
                                   ('strokeGradient', 'blendMode', 'MULTIPLY')):
            changed = copy.deepcopy(data)
            changed[kind][field] = value
            with self.subTest(kind=kind, field=field), self.assertRaises(ValueError):
                generator.search_material_defaults(changed)
        for index, field, value in ((0, 'visible', False), (0, 'blurType', 'LAYER'),
                                    (1, 'visible', False)):
            changed = copy.deepcopy(data)
            changed['effects'][index][field] = value
            with self.subTest(index=index, field=field), self.assertRaises(ValueError):
                generator.search_material_defaults(changed)

    def test_search_icon_preserves_figma_asset_and_tracks_the_lvui_color(self):
        source = (ROOT / 'resources/icons/searchField.svg').read_text(encoding='utf-8')
        binding = ' data-lvui-stroke-token="Theme Colors/Icon/search"'
        self.assertEqual(source.count(binding), 2)
        self.assertEqual(hashlib.sha256(source.replace(binding, '').encode()).hexdigest(),
                         'a1b6c7c46ca07510ec8f2087ba2cfa5eaedcd3413a803565413650b1aa8549d1')
        self.assertEqual(icons.resolve_svg_tokens(source, self.data).count('stroke="#CED0D6"'), 2)
        changed = copy.deepcopy(self.data)
        color = next(v for v in changed['variables'] if v['name'] == 'Icon/search')
        next(iter(color['valuesByMode'].values())).update(r=1., g=0., b=0.)
        self.assertEqual(icons.resolve_svg_tokens(source, changed).count('stroke="#FF0000"'), 2)

    def test_slider_symbol_preserves_figma_asset_and_tracks_its_lvui_color(self):
        source = (ROOT / 'resources/icons/sliderSymbolSun.svg').read_text(encoding='utf-8')
        binding = ' data-lvui-fill-token="Material/Alert/actionBorder"'
        self.assertEqual(source.count(binding), 8)
        self.assertEqual(hashlib.sha256(source.replace(binding, '').encode()).hexdigest(),
                         '652de0c8806be3d01dfc801f7c901be67888936e196890e8ce7a2f539c364e9f')
        self.assertEqual(icons.resolve_svg_tokens(source, self.data).count('fill="#596168"'), 8)
        changed = copy.deepcopy(self.data)
        color = next(v for v in changed['variables'] if v['name'] == 'Alert/actionBorder')
        next(iter(color['valuesByMode'].values())).update(r=1., g=0., b=0.)
        self.assertEqual(icons.resolve_svg_tokens(source, changed).count('fill="#FF0000"'), 8)

    def test_builtin_icons_reference_darker_roles(self):
        bindings = {'generaladd.svg': ('stroke', 'Blue', '#2373CD'),
                    'projectStructure.svg': ('fill', 'Blue', '#2373CD'),
                    'alertAdjustments.svg': ('stroke', 'Blue', '#2373CD'),
                    'alertFileText.svg': ('stroke', 'Blue', '#2373CD'),
                    'nodesfolder.svg': ('stroke', 'Yellow', '#CDAE30')}
        for name, (paint, accent, expected) in bindings.items():
            source = (ROOT / 'resources/icons' / name).read_text(encoding='utf-8')
            self.assertIn(f'data-lvui-{paint}-token="Theme Colors/Accent/{accent}/Hover"', source)
            self.assertIn(f'{paint}="{expected}"', icons.resolve_svg_tokens(source, self.data))


if __name__ == "__main__":
    unittest.main()
