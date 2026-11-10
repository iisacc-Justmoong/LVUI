#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ListItem(type: .ActionGroup, label: vm.label, primaryAction: vm.primary, secondaryAction: \"Edit\", onActionTriggered: vm.triggered) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeListItemComposite.h>","detail_list_composite::NativeListItemComposite::create(","lvui::ListItemVariant::action_group",
                   "lvui::native::list_footer_slot(","lvui::ListFooterSlot{.text = \"Edit\"}","const lvui::ListFooterSlot& value"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent list actions lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("List actions depend on legacy widgets");
    for(auto source:{"Window { ListItem(type: .Action) }","Window { ListItem(type: .ActionGroup, showMoreMenu: false) }",
                     "Window { ListItem(type: .detailActions) }"}){
        auto result=lvui::compile_native(source);if(!result||result->find("NativeListItemComposite::create")==result->npos)
            throw std::runtime_error("List action item was not generated");
    }
    auto detail=lvui::compile_native("import vm Model\nWindow { ListItem(type: .DetailActions, metadata1: vm.metadata, showMetadata: vm.visible, moreItems: [\"Rename\", \"Delete\"], onMenuItemTriggered: vm.menu) }");
    if(!detail)throw std::runtime_error(detail.error().message);
    for(auto part:{"ListItemVariant::detail_actions","value.row.configuration.description = \"Supporting text\"", "value.primary_action.text = \"Open\"",
                   "value.secondary_action.text = \"Edit\"","value.metadata1", "value.show_metadata", "list_item_menu_options", "const lvui::ListItemOption& item"})
        if(detail->find(part)==detail->npos)throw std::runtime_error(std::string("Detail item code generation lost ")+part);
    auto rich=lvui::compile_native("import vm Model\nWindow { ListItem(moreItems: vm.entries, onMenuSelectionTriggered: vm.selected) }");
    if(!rich||rich->find("assign_list_item_menu_options")==rich->npos||rich->find("const lvui::ListItemMenuSelection& value")==rich->npos)
        throw std::runtime_error("Rich menu input or callback did not select the compound runtime");
    for(auto source:{"Window { ListItem(type: .Action, primaryAction: 1) }","Window { ListItem(type: .Action, actionWidth: 0) }",
                     "Window { ListItem(type: .ActionGroup, showMoreMenu: 1) }", "Window { ListItem(type: .DetailActions, moreItems: [1]) }",
                     "Window { ListItem(type: .DetailActions, slotSpacing: 4, contentSpacing: 4) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid list action input was accepted");
}
