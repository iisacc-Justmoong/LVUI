#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { HelpButton(id: \"help\", text: vm.text, width: vm.width, enabled: vm.enabled, onClick: vm.open) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/HelpButton.h>","lvui::help_button_configuration()","lvui::native::ButtonBinding<","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated HelpButton lost ")+token);
    if(code->find("widgets.help_button(")!=code->npos||code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated HelpButton depends on a prebuilt or legacy control");
    auto motion=lvui::compile_native("import vm Model\nWindow { HelpButton(motionEnabled: vm.motion) }");
    if(!motion||motion->find("value.configuration.motion_enabled =")==motion->npos)
        throw std::runtime_error("HelpButton did not forward reactive motionEnabled");
    auto motion_off=lvui::compile_native("Window { HelpButton(motionEnabled: false) }");
    if(!motion_off||motion_off->find("value.configuration.motion_enabled = false")==motion_off->npos)
        throw std::runtime_error("HelpButton did not forward static motionEnabled");
    auto events=lvui::compile_native("import vm Model\nWindow { HelpButton(onPressed: vm.press, onReleased: vm.release, onCanceled: vm.cancel) }");
    if(!events||events->find("lvui::native::NativeButtonEvents{")==events->npos||events->find("->set_events(")==events->npos)
        throw std::runtime_error("HelpButton did not forward input events");
    for(auto source:{"Window { HelpButton(width: 0) }","Window { HelpButton(enabled: 1) }","Window { HelpButton(tone: .invalid) }","Window { HelpButton(motionEnabled: 1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid HelpButton input was accepted");
}
