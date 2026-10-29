from pathlib import Path
import importlib.util,shutil,tempfile,unittest
UI=Path(__file__).resolve().parents[1];RS=UI.parent/'LVRS'
spec=importlib.util.spec_from_file_location('native_render_contract',RS/'tools/native_render_contract.py');contract=importlib.util.module_from_spec(spec);spec.loader.exec_module(contract)
class RenderContract(unittest.TestCase):
    def copy(self):
        temporary=tempfile.TemporaryDirectory(prefix='render-contract-',dir=UI/'build');self.addCleanup(temporary.cleanup);root=Path(temporary.name)
        for file in ['src/backend/runtime/renderingmonitor.h','src/backend/runtime/renderingmonitor.cpp','src/LVRS/RenderMonitor.h','src/LVRS/RenderMonitor.cpp','tests/native/fixtures/render-monitor-contract.json']:
            target=root/file;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(RS/file,target)
        return root
    def test_surface(self):
        result=contract.audit();self.assertEqual([result[k] for k in ['properties','methods','setters']],[11,5,2]);self.assertFalse(result['native_runtime_executed']);self.assertFalse(result['original_runtime_executed'])
    def test_original_drift(self):
        root=self.copy();path=root/'src/backend/runtime/renderingmonitor.cpp';path.write_text(path.read_text()+'\n// drift\n')
        with self.assertRaisesRegex(ValueError,'Original render monitor source drift'):contract.audit(root)
    def test_class_scoped_api(self):
        root=self.copy();path=root/'src/LVRS/RenderMonitor.h';path.write_text(path.read_text().replace(' fps(',' removedFps(')+'\nstruct Other { void fps(); };\n')
        with self.assertRaisesRegex(ValueError,'Missing native render monitor API: fps'):contract.audit(root)
    def test_default(self):
        root=self.copy();path=root/'src/LVRS/RenderMonitor.cpp';path.write_text(path.read_text().replace('capacity=240','capacity=239'))
        with self.assertRaisesRegex(ValueError,'Native render monitor default differs: capacity'):contract.audit(root)
if __name__=='__main__':unittest.main()
