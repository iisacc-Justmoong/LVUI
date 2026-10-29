from pathlib import Path
import importlib.util,shutil,tempfile,unittest
UI=Path(__file__).resolve().parents[1];RS=UI.parent/'LVRS'
spec=importlib.util.spec_from_file_location('native_svg_contract',RS/'tools/native_svg_contract.py');contract=importlib.util.module_from_spec(spec);spec.loader.exec_module(contract)
class SvgContract(unittest.TestCase):
    def copy(self):
        temporary=tempfile.TemporaryDirectory(prefix='svg-contract-',dir=UI/'build');self.addCleanup(temporary.cleanup);root=Path(temporary.name)
        for file in ['src/backend/graphics/svgmanager.h','src/backend/graphics/svgmanager.cpp','src/LVRS/SvgManager.h','src/LVRS/SvgManager.cpp','tests/native/fixtures/svg-manager-contract.json']:
            target=root/file;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(RS/file,target)
        return root
    def test_surface(self):
        result=contract.audit();self.assertEqual([result[k] for k in ['properties','methods','setters']],[6,4,4]);self.assertFalse(result['native_runtime_executed']);self.assertFalse(result['original_runtime_executed'])
    def test_original_drift(self):
        root=self.copy();path=root/'src/backend/graphics/svgmanager.cpp';path.write_text(path.read_text()+'\n// drift\n')
        with self.assertRaisesRegex(ValueError,'Original SVG source drift'):contract.audit(root)
    def test_class_scoped_api(self):
        root=self.copy();path=root/'src/LVRS/SvgManager.h';path.write_text(path.read_text().replace(' icon(',' removedIcon(')+'\nstruct Other { void icon(); };\n')
        with self.assertRaisesRegex(ValueError,'Missing native SVG API: icon'):contract.audit(root)
    def test_default(self):
        root=self.copy();path=root/'src/LVRS/SvgManager.cpp';path.write_text(path.read_text().replace('cache_size=256','cache_size=255'))
        with self.assertRaisesRegex(ValueError,'Native SVG default differs: cache_size'):contract.audit(root)
if __name__=='__main__':unittest.main()
