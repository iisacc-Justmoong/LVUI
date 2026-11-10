#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto result=lvui::compile_native("import vm Model\nWindow { FocusRing(width: 120, height: 36, active: vm.focused, motionEnabled: false) }");
    if(!result)throw std::runtime_error(result.error().message);
    for(auto token:{"#include <iiGUI/NativeFocusRingView.h>","ii::gui::NativeFocusRing::create(","component.set_options(value)"})
        if(result->find(token)==result->npos)throw std::runtime_error(std::string("FocusRing lost its iiGUI binding: ")+token);
    if(lvui::compile_native("Window { FocusRing(width: -1) }"))throw std::runtime_error("Invalid focus-ring extent was accepted");
}
