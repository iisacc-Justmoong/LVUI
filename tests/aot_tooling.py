"""AOT command-line/build artifact contract; no Python in generated apps."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

compiler = Path(sys.argv.pop(1)).resolve()
build_root = Path(sys.argv.pop(1)).resolve()
runner = Path(sys.argv.pop(1)).resolve()


class AotToolingTests(unittest.TestCase):
    def test_shared_profile_compiles_views_and_native_styles(self):
        profile = self.root / 'shared theme.lvui'
        profile.write_text('let window: Color = Color("#141719")\nlet inspectorWidth: Metric = 352\n', encoding='utf-8')
        source = 'Window { VStack(width: Metric.inspectorWidth, backgroundColor: .window) {} }'
        view = self.invoke(source, '--style-file', str(profile))
        self.assertEqual(view.returncode, 0, view.stderr)
        self.assertIn('set_stack_layout(', view.stdout)
        self.assertIn('.extent = {352, 0}', view.stdout)
        self.assertNotIn('LVUI/', view.stdout)
        output = self.root / 'theme.generated.h'
        result = subprocess.run(['cmake', f'-DLVUI_AOT_COMPILER={compiler}', f'-DLVUI_AOT_SOURCE={profile}',
                                 f'-DLVUI_AOT_OUTPUT={output}', '-DLVUI_AOT_EMIT_STYLES=ON',
                                 '-DLVUI_AOT_NAMESPACE=Example.Style', '-P', str(runner)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('namespace Example::Style', output.read_text())
        self.assertIn('inspectorWidth = 352;', output.read_text())
        profile.write_text('let inspectorWidth: Metric = 400\n', encoding='utf-8')
        regenerated = self.invoke(source, '--style-file', str(profile))
        self.assertEqual(regenerated.returncode, 0, regenerated.stderr)
        self.assertIn('.extent = {400, 0}', regenerated.stdout)

    def test_invalid_profile_preserves_previous_artifact(self):
        profile = self.root / 'invalid theme.lvui'
        profile.write_text('let width: Metric = Color.primary', encoding='utf-8')
        output = self.root / 'previous generated.h'
        output.write_text('previous valid artifact', encoding='utf-8')
        result = subprocess.run(['cmake', f'-DLVUI_AOT_COMPILER={compiler}', f'-DLVUI_AOT_SOURCE={profile}',
                                 f'-DLVUI_AOT_OUTPUT={output}', '-DLVUI_AOT_EMIT_STYLES=ON', '-P', str(runner)],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(output.read_text(), 'previous valid artifact')
        invalid = self.invoke('Window {}', '--style-file', str(profile))
        self.assertEqual(invalid.returncode, 1)
        self.assertEqual(invalid.stdout, '')
        self.assertIn(f'{profile}:1:', invalid.stderr)

    def test_profile_limits_and_namespace_are_checked(self):
        invalid = self.invoke('let x: Metric = 1', '--emit-styles', '--namespace', 'bad;namespace')
        self.assertEqual(invalid.returncode, 1)
        self.assertEqual(invalid.stdout, '')
        missing = self.invoke('Window {}', '--style-file', str(self.root / 'missing.lvui'))
        self.assertEqual(missing.returncode, 2)
        self.assertEqual(missing.stdout, '')
        huge = self.invoke(' ' * (1024 * 1024 + 1), '--emit-styles')
        self.assertEqual(huge.returncode, 1)
        self.assertEqual(huge.stdout, '')

    def test_style_types_are_resolved_before_emission(self):
        result=self.invoke('let gap: Metric = .space8\nWindow { VStack(spacing: gap) { Label(color: .primary) } }')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('.spacing = 8',result.stdout)
        self.assertNotIn('Color.primary',result.stdout)
        failed=self.invoke('Window {\n Label(color: Color.unknown)\n}')
        self.assertEqual(failed.returncode,1)
        self.assertEqual(failed.stdout,'')
        self.assertIn(':2:',failed.stderr)
    def test_style_modes_are_explicit_and_checked(self):
        source='Window { Label(text: token("Theme Typography/Font/fontBody")) }'
        valid=self.invoke(source,'--style-mode','Theme Colors=Dark','--style-mode','Theme Typography=Value')
        self.assertEqual(valid.returncode,0,valid.stderr)
        self.assertIn('"Pretendard"',valid.stdout)
        for choice in ['Theme Colors=Light','missing=Dark']:
            invalid=self.invoke(source,'--style-mode',choice)
            self.assertEqual(invalid.returncode,1)
            self.assertEqual(invalid.stdout,'')
        for choice in ['Theme Colors','Theme Colors=','=Dark']:
            self.assertEqual(self.invoke(source,'--style-mode',choice).returncode,2)
    def test_native_lvrs_framework_cli(self):
        result=self.invoke('ApplicationWindow(accent: .green) { CheckBox(checked: true) }','--framework','LVRS')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('lvrs::Widgets controls',result.stdout)
        self.assertIn('lvrs::Accent::green',result.stdout)
        rejected=self.invoke('ApplicationWindow { Label() }','--framework','unknown')
        self.assertEqual(rejected.returncode,1)
        self.assertEqual(rejected.stdout,'')
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='aot tooling ', dir=build_root)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'a view.lvui'

    def invoke(self, source, *arguments):
        self.source.write_text(source, encoding='utf-8')
        return subprocess.run([str(compiler), *arguments, str(self.source)], capture_output=True, text=True)

    def test_success_emits_only_cpp_and_deterministic_escaped_unicode(self):
        source = 'ApplicationWindow { Label(text: "Hello \\\"Cocoa\\\" 한글") }'
        result = self.invoke(source, '--name', 'Hello')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, '')
        self.assertIn('struct Hello final', result.stdout)
        self.assertIn('\\355', result.stdout)
        self.assertEqual(result.stdout, self.invoke(source, '--name', 'Hello').stdout)
        self.assertNotIn('lvui::parse', result.stdout)

    def test_semantic_error_has_position_and_no_partial_code(self):
        result = self.invoke('ApplicationWindow {\n Label(text: false)\n}')
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')
        self.assertIn(':2:8:', result.stderr)
        self.assertIn('Expected String', result.stderr)

    def test_invalid_arguments_and_injection_never_emit_code(self):
        for arguments in [('--name', 'A;code'), ('--include', 'evil".h'), ('--include', '../escape.h')]:
            result = self.invoke('ApplicationWindow { Label() }', *arguments)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(result.stdout, '')
        result = subprocess.run([str(compiler), '--unknown'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)

    def test_input_limit_is_enforced_before_output(self):
        result = self.invoke(' ' * (1024 * 1024 + 1))
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')
        self.assertIn('1 MiB', result.stderr)

    def test_failed_build_generation_preserves_previous_output(self):
        self.source.write_text('ApplicationWindow { Label(text: false) }', encoding='utf-8')
        output = self.root / 'existing generated.h'
        output.write_text('previous valid artifact', encoding='utf-8')
        result = subprocess.run(['cmake', f'-DLVUI_AOT_COMPILER={compiler}', f'-DLVUI_AOT_SOURCE={self.source}',
                                 f'-DLVUI_AOT_OUTPUT={output}', '-DLVUI_AOT_NAME=Hello', '-P', str(runner)],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(output.read_text(), 'previous valid artifact')

    def test_unchanged_build_generation_preserves_artifact_timestamp(self):
        for styles in (False, True):
            with self.subTest(styles=styles):
                initial = 'let gap: Metric = 8' if styles else 'Window { Label(text: "First") }'
                changed = 'let gap: Metric = 16' if styles else 'Window { Label(text: "Second") }'
                self.source.write_text(initial, encoding='utf-8')
                output = self.root / 'stable generated.h'
                arguments = ['cmake', f'-DLVUI_AOT_COMPILER={compiler}',
                             f'-DLVUI_AOT_SOURCE={self.source}', f'-DLVUI_AOT_OUTPUT={output}']
                if styles:
                    arguments.append('-DLVUI_AOT_EMIT_STYLES=ON')
                arguments.extend(['-P', str(runner)])
                result = subprocess.run(arguments, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                previous = output.read_bytes()
                sentinel = 946684800000000000
                os.utime(output, ns=(sentinel, sentinel))
                result = subprocess.run(arguments, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(output.read_bytes(), previous)
                self.assertEqual(output.stat().st_mtime_ns, sentinel,
                                 'Identical native output triggered dependent C++ rebuilds')
                self.source.write_text(changed, encoding='utf-8')
                result = subprocess.run(arguments, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertNotEqual(output.read_bytes(), previous)
                self.assertNotEqual(output.stat().st_mtime_ns, sentinel)


unittest.main()
