#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

namespace { void require(bool value,const char* message){if(!value)throw std::runtime_error(message);} }
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ToggleSwitch(text: vm.text, checked: vm.checked, enabled: vm.enabled, trackWidth: vm.width, rightToLeft: vm.rtl, motion: vm.motion, onColor: vm.on_color, onToggled: vm.changed) }");
    require(bool(code),"Independent toggle did not compile");
    require(code->find("#include <LVUI/NativeToggleSwitch.h>")!=code->npos&&
            code->find("lvui::native::NativeToggleSwitch::create(")!=code->npos&&
            code->find("lvui::ToggleSwitchConfiguration{")!=code->npos&&
            code->find("component.set_options(value)")!=code->npos&&code->find("lvui::native::toggle_color(")!=code->npos&&
            code->find("widgets.toggle_switch(")==code->npos&&code->find("lvrs::")==code->npos,
            "Toggle still depends on the host or legacy implementation");
    auto invalid=lvui::compile_native("Window { ToggleSwitch(trackWidth: -1) }");
    require(!invalid&&invalid.error().line>0,"Invalid static switch width was accepted");
}
