#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ListItem(type: .Task, label: vm.label, checked: vm.checked, selector: vm.priorities, selectorIndex: vm.priority, quantity: vm.quantity, onCheckedChanged: vm.checkedChanged, onSelectorIndexChanged: vm.priorityChanged, onQuantityChanged: vm.quantityChanged) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"lvui::ListItemVariant::task","detail_list_composite::NativeListItemComposite::create(",
                   "value.primary_action.text = \"Done\"","value.secondary_action.text = \"Assign\"",
                   "value.selector", "value.quantity", ".checked_changed", ".selector_index_changed", ".quantity_changed"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Task generation lost ")+part);
    auto lowercase=lvui::compile_native("Window { ListItem(type: .task) }");
    if(!lowercase||lowercase->find("NativeListItemComposite::create")==lowercase->npos)
        throw std::runtime_error("Lowercase task did not use the compound runtime");
    auto dynamic=lvui::compile_native("import vm Model\nWindow { ListItem(type: vm.kind) }");
    if(!dynamic||dynamic->find("list_item_default_primary")==dynamic->npos||dynamic->find("list_item_default_secondary")==dynamic->npos)
        throw std::runtime_error("Dynamic task type did not resolve its default actions");
    for(auto source:{"Window { ListItem(type: .Task, selector: [true]) }",
                     "Window { ListItem(type: .Task, showSelection: 1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid task input was accepted");
}
