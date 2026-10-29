#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ListItem(type: .Toggle, label: vm.label, checked: vm.checked, toggleEnabled: vm.enabled, onCheckedChanged: vm.changed, onEdited: vm.edited) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeListItemComposite.h>","lvui::native::NativeListItemComposite::create(","lvui::native::ListItemCompositeInput",
                   "lvui::ListItemVariant::toggle","component.set_options(value)","const lvui::ListItemValue& value"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent compound list item lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Compound list item depends on legacy widgets");
    for(auto source:{"Window { ListItem(type: .Toggle) }","Window { ListItem(type: .Checkable, checked: true) }",
                     "import vm Model\nWindow { ListItem(type: vm.kind) }"}){
        auto result=lvui::compile_native(source);if(!result||result->find("NativeListItemComposite::create")==result->npos)
            throw std::runtime_error("Compound list item was not generated");
    }
    for(auto source:{"Window { ListItem(type: .Toggle, checked: 1) }","Window { ListItem(type: .Checkable, slotSpacing: -1) }",
                     "Window { ListItem(type: .DetailActions) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid compound list item input was accepted");
}
