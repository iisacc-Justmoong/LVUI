#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { WindowNoiseTexture(width: 200, height: 100, noiseStrength: vm.strength, blurRadius: 1.5, grainStrength: 0.1) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <iiGUI/NativeWindowNoiseTextureView.h>","ii::gui::NativeWindowNoiseTexture::create(","component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Window noise lost its iiGUI implementation: ")+part);
    if(lvui::compile_native("Window { WindowNoiseTexture(noiseStrength: 2) }"))throw std::runtime_error("Invalid noise strength was accepted");
}
