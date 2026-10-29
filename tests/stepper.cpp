#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { Stepper(id: \"stepper\", arrow: vm.arrow, tone: vm.tone, enabled: vm.enabled, cornerRadius: vm.radius, onStepped: vm.step, onClick: vm.clicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeStepper.h>","lvui::native::NativeStepper::create(","lvui::StepperConfiguration","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated Stepper lost ")+token);
    if(code->find("widgets.stepper(")!=code->npos||code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated Stepper depends on a prebuilt or legacy control");
    for(auto source:{"Window { Stepper(arrow: .Sideways) }","Window { Stepper(tone: .Wrong) }",
                     "Window { Stepper(width: 0) }","Window { Stepper(enabled: 1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid Stepper input was accepted");
    auto legacy=lvui::compile_native("Window { Stepper(arrow: .up) }",{.framework="LVRS"});
    if(!legacy||legacy->find("controls.stepper(")==legacy->npos)throw std::runtime_error("Explicit legacy Stepper path was altered");
}
