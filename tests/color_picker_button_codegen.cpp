#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ColorPickerButton(model: vm.source, modelRevision: vm.revision, buttonSize: vm.size, showHueRing: vm.ring, onClick: vm.clicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeColorPickerButton.h>","lvui::native::NativeColorPickerButton::create(","lvui::native::ColorPickerButtonInput","component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent ColorPickerButton lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default ColorPickerButton depends on a legacy implementation");
    for(auto source:{"Window { ColorPickerButton(buttonSize: .Huge) }","Window { ColorPickerButton(buttonSize: 7) }",
        "Window { ColorPickerButton(currentColor: \"bad color\") }","Window { ColorPickerButton(modelRevision: 1) }",
        "import vm Model\nWindow { ColorPickerButton(model: vm.source, currentColor: \"#FF0000\") }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid ColorPickerButton input was accepted");
}
