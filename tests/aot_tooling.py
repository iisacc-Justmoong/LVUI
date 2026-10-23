"""AOT command-line/build artifact contract; no Python in generated apps."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

compiler = Path(sys.argv.pop(1)).resolve()
build_root = Path(sys.argv.pop(1)).resolve()
runner = Path(sys.argv.pop(1)).resolve()


class AotToolingTests(unittest.TestCase):
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


unittest.main()
