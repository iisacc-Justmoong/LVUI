import Dreamscapes.Storage 1.0
        Item {
            NativeToolState { }
            Repeater { model: vm.rows; LV.Label { text: modelData.name } }
            Loader { active: vm.ready }
        }