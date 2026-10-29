from pathlib import Path
import importlib.util,shutil,tempfile,unittest
UI=Path(__file__).resolve().parents[1];RS=UI.parent/'LVRS';GUI=UI.parent/'iiGUI'
spec=importlib.util.spec_from_file_location('native_font_contract',RS/'tools/native_font_contract.py');contract=importlib.util.module_from_spec(spec);spec.loader.exec_module(contract)
class FontContract(unittest.TestCase):
    def copy(self):
        temporary=tempfile.TemporaryDirectory(prefix='font-contract-',dir=UI/'build');self.addCleanup(temporary.cleanup);root=Path(temporary.name)/'LVRS'
        for file in ['src/backend/fonts/fontpolicy.h','src/backend/fonts/fontpolicy.cpp','src/LVRS/FontPolicy.h','src/LVRS/FontPolicy.cpp','tests/native/fixtures/font-policy-contract.json']:
            target=root/file;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(RS/file,target)
        shutil.copytree(RS/'resources/font',root/'resources/font')
        return root
    def test_surface(self):
        report=contract.audit();self.assertEqual([report[key] for key in ['properties','methods','statics','tokens','substitutions','fonts']],[5,6,3,7,8,10]);self.assertFalse(report['native_runtime_executed'])
    def test_source_drift(self):
        root=self.copy();path=root/'src/backend/fonts/fontpolicy.cpp';path.write_text(path.read_text()+'\n// changed\n')
        with self.assertRaisesRegex(ValueError,'Original font policy drift'):contract.audit(root,GUI)
    def test_missing_api(self):
        root=self.copy();path=root/'src/LVRS/FontPolicy.h';path.write_text(path.read_text().replace('weightForTextSize(','removedWeight('))
        with self.assertRaisesRegex(ValueError,'Missing native font API: weightForTextSize'):contract.audit(root,GUI)
    def test_missing_substitution(self):
        root=self.copy();path=root/'src/LVRS/FontPolicy.cpp';path.write_text(path.read_text().replace('"SF Pro Display",',''))
        with self.assertRaisesRegex(ValueError,'Native font substitution targets differ'):contract.audit(root,GUI)
if __name__=='__main__':unittest.main()
