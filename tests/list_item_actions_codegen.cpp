#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ListItem(type: .ActionGroup, label: vm.label, primaryAction: vm.primary, secondaryAction: \"Edit\", onActionTriggered: vm.triggered) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeListItemComposite.h>","lvui::native::NativeListItemComposite::create(","lvui::ListItemVariant::action_group",
                   "lvui::native::list_footer_slot(","lvui::ListFooterSlot{.text = \"Edit\"}","const lvui::ListFooterSlot& value"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent list actions lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("List actions depend on legacy widgets");
    for(auto source:{"Window { ListItem(type: .Action) }","Window { ListItem(type: .ActionGroup, showMoreMenu: false) }"}){
        auto result=lvui::compile_native(source);if(!result||result->find("NativeListItemComposite::create")==result->npos)
            throw std::runtime_error("List action item was not generated");
    }
    for(auto source:{"Window { ListItem(type: .Action, primaryAction: 1) }","Window { ListItem(type: .Action, actionWidth: 0) }",
                     "Window { ListItem(type: .ActionGroup, showMoreMenu: 1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid list action input was accepted");
}
