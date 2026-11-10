#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { RadioButton(text: vm.text, checked: vm.checked, group: vm.group, indicatorSize: vm.size, onColor: vm.color, onToggled: vm.changed) }");
    require(bool(code),"Independent radio did not compile");
    require(code->find("#include <LVUI/NativeRadioButton.h>")!=code->npos&&
            code->find("lvui::native::RadioButtonBinding<ii::gui::BasicRadioButton, ii::gui::BasicNativeRadioButton>::create(")!=code->npos&&
            code->find("#include <iiGUI/NativeRadioButtonView.h>")!=code->npos&&
            code->find("lvui::RadioButtonConfiguration{")!=code->npos&&
            code->find("component.set_options(value)")!=code->npos&&
            code->find("widgets.radio_button(")==code->npos&&code->find("lvrs::")==code->npos,
            "Radio application composition is missing");
    require(code->find(".motion = lvui::Motion{.duration = std::chrono::milliseconds{360}, .curve = lvui::MotionCurve::out_back")!=code->npos,
            "Generated radio did not use the LVRS default dot transition");
    auto custom_motion=lvui::compile_native("Window { RadioButton(transitionDuration: 125) }");
    require(custom_motion&&custom_motion->find("radio_motion_duration(")!=custom_motion->npos&&
            custom_motion->find(", 125)")!=custom_motion->npos,"An explicit radio transition duration was not preserved");
    auto invalid=lvui::compile_native("Window { RadioButton(indicatorSize: -1) }");
    require(!invalid&&invalid.error().line>0,"Invalid static radio size was accepted");
    auto plain=lvui::compile_native("Window { RadioButton(group: \"choice\", state: true, available: false) }");
    require(plain&&plain->find("std::make_shared<ii::gui::RadioGroups>()")!=plain->npos&&
            plain->find(".checked = true")!=plain->npos&&plain->find(".enabled = false")!=plain->npos,
            "A static radio did not own its group or compatibility inputs");
    require(!lvui::compile_native("Window { RadioButton(checked: true, state: false) }")&&
            !lvui::compile_native("Window { RadioButton(enabled: true, available: false) }"),
            "Conflicting radio aliases were accepted");
}
