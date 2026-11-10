#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ListItem(type: .Toggle, label: vm.label, checked: vm.checked, toggleEnabled: vm.enabled, onCheckedChanged: vm.changed, onEdited: vm.edited) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeListItemComposite.h>","detail_list_composite::NativeListItemComposite::create(","detail_list_composite::Input",
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
                     "Window { ListItem(type: .Unsupported) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid compound list item input was accepted");
    for(auto source:{"Window { ListItemComposite(label: \"Resource\") }",
                     "Window { ListItemComposite(type: .Toggle, checked: true) }"}){
        auto result=lvui::compile_native(source);
        if(!result||result->find("detail_list_composite::NativeListItemComposite::create(")==result->npos)
            throw std::runtime_error("ListItemComposite did not compose the iiGUI component");
    }
    if(lvui::compile_native("Window { ListItemComposite(type: .Toggle, checked: 1) }"))
        throw std::runtime_error("ListItemComposite accepted an invalid checked value");
}
