#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ListItem(type: .InlineEdit, label: vm.label, inputText1: vm.text, inputReadOnly1: vm.read_only, onInputTextChanged: vm.changed, onInputSubmitted: vm.submitted, onEdited: vm.edited) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeListItemComposite.h>","lvui::native::NativeListItemComposite::create(","lvui::ListItemVariant::inline_edit",
                   "value.input_text", ".input_text_changed =", ".input_submitted =", "component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent inline item lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Inline item depends on legacy widgets");
    for(auto source:{"Window { ListItem(type: .InlineEdit) }","Window { ListItem(type: .inlineEdit, inputText1: \"Name\", showInput1: false) }"}){
        auto result=lvui::compile_native(source);if(!result||result->find("NativeListItemComposite::create")==result->npos)
            throw std::runtime_error("Inline item was not generated");
    }
    for(auto source:{"Window { ListItem(type: .InlineEdit, inputText1: 1) }","Window { ListItem(type: .InlineEdit, inputWidth: 0) }",
                     "Window { ListItem(type: .InlineEdit, showInput1: 1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid inline item input was accepted");
}
