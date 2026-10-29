#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { HelpButton(id: \"help\", text: vm.text, width: vm.width, enabled: vm.enabled, onClick: vm.open) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/HelpButton.h>","lvui::help_button_configuration()","lvui::native::NativeButton::create(","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated HelpButton lost ")+token);
    if(code->find("widgets.help_button(")!=code->npos||code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated HelpButton depends on a prebuilt or legacy control");
    for(auto source:{"Window { HelpButton(width: 0) }","Window { HelpButton(enabled: 1) }","Window { HelpButton(tone: .invalid) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid HelpButton input was accepted");
}
