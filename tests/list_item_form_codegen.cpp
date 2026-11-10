#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ListItem(type: .Form, fieldLabel1: \"Title\", fieldLabel2: \"Notes\", inputText1: vm.title, inputText2: vm.notes, inputReadOnly2: vm.locked, onInputTextChanged: vm.titleChanged, onInputText2Changed: vm.notesChanged, onInput2Submitted: vm.notesSubmitted, onActionValuesTriggered: vm.actionValues, onActionEvent: vm.actionEvent) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"lvui::ListItemVariant::form","detail_list_composite::NativeListItemComposite::create(","value.field_label1", "value.field_label2",
                   "value.input_text2", "value.input2_read_only", ".input_text2_changed", ".input_submitted2", ".action_values_triggered", ".action_event",
                   "value.primary_action.text = \"Save\"","value.secondary_action.text = \"Cancel\""})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Form generation lost ")+part);
    auto lowercase=lvui::compile_native("Window { ListItem(type: .form) }");
    if(!lowercase||lowercase->find("NativeListItemComposite::create")==lowercase->npos)
        throw std::runtime_error("Lowercase form did not use the compound runtime");
    auto callback_only=lvui::compile_native("import vm Model\nWindow { ListItem(onActionEvent: vm.event) }");
    if(!callback_only||callback_only->find("NativeListItemComposite::create")==callback_only->npos||callback_only->find(".action_event")==callback_only->npos)
        throw std::runtime_error("A unified action callback did not select the compound runtime");
    auto footer=lvui::compile_native("import vm Model\nWindow { ListItem(type: .Form, footerComponent: PushButton(text: vm.label)) }");
    if(!footer||footer->find("NativeListItemComposite::create")==footer->npos||footer->find(".footer_context = detail_list_composite::ViewFactory")==footer->npos)
        throw std::runtime_error("List item custom footer did not become a scoped native view");
    auto forwarded=lvui::compile_native("import vm Model\nlet Row(content: View): View = ListItem(type: .Form, footerComponent: content)\nWindow { Row(content: Label(text: vm.label)) }");
    if(!forwarded||forwarded->find(".footer_context = detail_list_composite::ViewFactory")==forwarded->npos)
        throw std::runtime_error("A forwarded footer View lost its lexical owner");
    auto contextual=lvui::compile_native("Window { ListItem(type: .Toggle, footerComponent: VStack {\nLabel(text: listItem.label)\nPushButton(onClick: callback(listItem.editValue(\"checked\", true)))\n}) }");
    if(!contextual)throw std::runtime_error(contextual.error().message);
    if(contextual->find("ListItemContext")==contextual->npos||contextual->find(".footer_context")==contextual->npos)
        throw std::runtime_error("List item custom view did not receive its owning context");
    if(lvui::compile_native("Window { Label(text: listItem.label) }"))
        throw std::runtime_error("List item context leaked outside an owned custom view");
    auto preview=lvui::compile_native("Window { ListItem(type: .Resource, previewComponent: Label(text: \"Preview\"), footerComponent: Label(text: \"Footer\"), leadingComponent: Label(text: \"Leading\"), trailingComponent: Label(text: \"Trailing\"), trailingWidth: 64, trailingHeight: 20) }");
    if(!preview||preview->find(".preview_context = detail_list_composite::ViewFactory")==preview->npos||preview->find(".footer_context = detail_list_composite::ViewFactory")==preview->npos||
       preview->find(".leading_context = detail_list_composite::ViewFactory")==preview->npos||preview->find(".trailing_context = detail_list_composite::ViewFactory")==preview->npos||
       preview->find("value.trailing_width")==preview->npos||preview->find("value.trailing_height")==preview->npos)
        throw std::runtime_error("Custom preview, leading, trailing and footer did not all become scoped native views");
    for(auto source:{"Window { ListItem(type: .Form, inputText2: 1) }",
                     "Window { ListItem(type: .Form, inputReadOnly2: 1) }",
                     "Window { ListItem(type: .Form, footerComponent: 3) }",
                     "Window { ListItem(type: .Resource, previewComponent: 3) }",
                     "Window { ListItem(type: .Resource, previewComponent: [Label(), Label()]) }",
                     "Window { ListItem(type: .Navigation, leadingComponent: 3) }",
                     "Window { ListItem(type: .Navigation, leadingComponent: [Label(), Label()]) }",
                     "Window { ListItem(type: .Navigation, trailingComponent: 3) }",
                     "Window { ListItem(type: .Navigation, trailingComponent: [Label(), Label()]) }",
                     "Window { ListItem(type: .Navigation, trailingWidth: -1) }",
                     "Window { ListItem(type: .Form, footerComponent: [Label(), Label()]) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid form input was accepted");
}
