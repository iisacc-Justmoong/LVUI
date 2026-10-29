from pathlib import Path
import importlib.util
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
LVRS = ROOT.parent / 'LVRS'
spec = importlib.util.spec_from_file_location('platform_contract', LVRS / 'tools/native_platform_contract.py')
contract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contract)


class PlatformContractTests(unittest.TestCase):
    def test_complete_declared_surface_and_profiles(self):
        fixture, summary = contract.audit(LVRS)
        self.assertEqual(summary, dict(legacy_properties=18, legacy_queries=9, profile_fields=41, profiles=7, aliases=22))
        self.assertEqual(fixture['profiles']['macos']['adaptiveWideBreakpoint'], 1040)
        self.assertEqual(fixture['profiles']['ios']['adaptiveWideBreakpoint'], 948)
        self.assertFalse(fixture['profiles']['wasm']['adaptiveAnimatedTransitions'])
        self.assertFalse(fixture['profiles']['unknown']['known'])

    def test_oracle_is_native_data_without_runtime_parsing(self):
        fixture, _ = contract.audit(LVRS)
        text = contract.render(fixture)
        self.assertEqual(text.count('if (target=='), 7)
        self.assertIn('ii::core::Value::Object', text)
        self.assertNotIn('parse(', text)
        self.assertNotIn('QVariant', text)

    def copy_root(self, root):
        for name in ['src/backend/platform/platforminfo.h', 'src/backend/platform/platforminfo.cpp',
                     'src/LVRS/PlatformInfo.h', 'src/LVRS/PlatformInfo.cpp', 'tests/native/fixtures/platform-profiles.json']:
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(LVRS / name, target)

    def test_source_drift_requires_a_new_review(self):
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            root = Path(temporary)
            self.copy_root(root)
            path = root / 'src/backend/platform/platforminfo.cpp'
            path.write_bytes(path.read_bytes() + b'\n')
            with self.assertRaisesRegex(ValueError, 'Legacy platform cpp changed'):
                contract.audit(root)

    def test_missing_native_key_is_rejected(self):
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as temporary:
            root = Path(temporary)
            self.copy_root(root)
            path = root / 'src/LVRS/PlatformInfo.cpp'
            path.write_text(path.read_text().replace('put("bootstrapMsaaSamples"', 'put("wrongField"'))
            with self.assertRaisesRegex(ValueError, 'metadata keys differ'):
                contract.audit(root)


if __name__ == '__main__':
    unittest.main()
