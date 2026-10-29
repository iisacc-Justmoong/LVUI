#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ListItem(type: .Select, label: vm.label, selector: vm.options, selectorIndex: vm.index, onSelectorIndexChanged: vm.changed, onEdited: vm.edited) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeListItemComposite.h>","lvui::native::NativeListItemComposite::create(","lvui::ListItemVariant::select",
                   "lvui::native::list_item_selector(",".selector_index_changed =", "component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent list selector lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("List selector depends on legacy widgets");
    for(auto source:{"Window { ListItem(type: .Select) }","Window { ListItem(type: .select, selector: [\"Draft\", \"Published\"], selectorIndex: 1) }"}){
        auto result=lvui::compile_native(source);if(!result||result->find("NativeListItemComposite::create")==result->npos)
            throw std::runtime_error("Selectable list item was not generated");
    }
    for(auto source:{"Window { ListItem(type: .Select, selector: [1]) }","Window { ListItem(type: .Select, selectorIndex: -1) }",
                     "Window { ListItem(type: .Select, showSelector: 1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid selectable list item input was accepted");
}
