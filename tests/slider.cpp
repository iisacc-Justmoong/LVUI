#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { Slider(id: \"slider\", from: 0, to: vm.maximum, value: vm.value, stepSize: 1, width: vm.width, enabled: vm.enabled, onMoved: vm.moved) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeSlider.h>","lvui::native::NativeSlider::create(","lvui::SliderConfiguration","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated Slider lost ")+token);
    if(code->find("widgets.slider(")!=code->npos||code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated Slider depends on a prebuilt or legacy control");
    for(auto source:{"Window { Slider(from: 1, to: 0) }","Window { Slider(width: 1) }",
                     "Window { Slider(thumbDiameter: 0) }","Window { Slider(trackHeight: -1) }",
                     "Window { Slider(stepSize: -1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid Slider input was accepted");
}
