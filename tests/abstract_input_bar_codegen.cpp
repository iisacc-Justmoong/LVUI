#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { AbstractInputBar(text: vm.text, placeholder: \"Draft\", width: 180) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <iiGUI/NativeInputFieldView.h>","detail_input::NativeInputField::create(","component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Input bar lost its iiGUI field: ")+part);
    if(lvui::compile_native("Window { AbstractInputBar(width: -1) }"))throw std::runtime_error("Invalid input-bar width was accepted");
}
