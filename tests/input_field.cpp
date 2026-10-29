#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { InputField(id: \"field\", text: vm.text, placeholder: vm.placeholder, width: vm.width, enabled: vm.enabled, readOnly: vm.readOnly, password: vm.password, onEdited: vm.edited, onSubmit: vm.submit) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeInputField.h>","lvui::native::NativeInputField::create(","lvui::InputFieldConfiguration","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated input field lost ")+token);
    if(code->find("widgets.input_field(")!=code->npos||code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated input field depends on a prebuilt or legacy control");
    for(auto source:{"Window { InputField(width: 0) }","Window { InputField(height: 0) }",
                     "Window { InputField(fontSize: 0) }","Window { InputField(enabled: 1) }",
                     "Window { InputField(backgroundColor: \"invalid\") }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid input field option was accepted");
    auto legacy=lvui::compile_native("Window { InputField(text: \"Legacy\") }",{.framework="LVRS"});
    if(!legacy||legacy->find("controls.input_field(")==legacy->npos)throw std::runtime_error("Explicit legacy input path was altered");
}
