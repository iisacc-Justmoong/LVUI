from pathlib import Path
import importlib.util, shutil, tempfile, unittest

UI=Path(__file__).resolve().parents[1]
LVRS=UI.parent/'LVRS'
module=importlib.util.spec_from_file_location('native_model_source_contract',LVRS/'tools/native_model_source_contract.py')
contract=importlib.util.module_from_spec(module);module.loader.exec_module(contract)

class ModelSourceContract(unittest.TestCase):
    def copy(self):
        temporary=tempfile.TemporaryDirectory(prefix='model-source-contract-',dir=UI/'build')
        self.addCleanup(temporary.cleanup);root=Path(temporary.name)
        for relative in ['tests/native/fixtures/model-source-contract.json','src/backend/model/modelsource.h','src/backend/model/modelsource.cpp','src/backend/navigation/modeladapter.h','src/backend/navigation/modeladapter.cpp','src/LVRS/ModelSource.h','src/LVRS/ModelAdapter.h']:
            target=root/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(LVRS/relative,target)
        return root
    def test_surface(self):
        result=contract.audit();self.assertEqual((result['ModelSource_properties'],result['ModelSource_methods'],result['ModelAdapter_methods']),(5,7,3))
        self.assertFalse(result['native_runtime_executed']);self.assertFalse(result['original_runtime_executed'])
    def test_original_drift(self):
        root=self.copy();path=root/'src/backend/model/modelsource.cpp';path.write_text(path.read_text()+'\n// changed\n')
        with self.assertRaisesRegex(ValueError,'Original source drift'):contract.audit(root)
    def test_missing_source_method(self):
        root=self.copy();path=root/'src/LVRS/ModelSource.h';path.write_text(path.read_text().replace('textValue(','removedText('))
        with self.assertRaisesRegex(ValueError,'Missing native API: ModelSource.textValue'):contract.audit(root)
        root=self.copy();path=root/'src/LVRS/ModelSource.h';path.write_text(path.read_text().replace('bool itemModel()const;','bool removedItemModel()const;'))
        with self.assertRaisesRegex(ValueError,'Missing native API: ModelSource.itemModel'):contract.audit(root)
    def test_missing_adapter_method(self):
        root=self.copy();path=root/'src/LVRS/ModelAdapter.h';path.write_text(path.read_text().replace('isItemModel(','removedType('))
        with self.assertRaisesRegex(ValueError,'Missing native API: ModelAdapter.isItemModel'):contract.audit(root)

if __name__=='__main__':unittest.main()
