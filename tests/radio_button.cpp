#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { RadioButton(text: vm.text, checked: vm.checked, group: vm.group, indicatorSize: vm.size, onColor: vm.color, onToggled: vm.changed) }");
    require(bool(code),"Independent radio did not compile");
    require(code->find("#include <LVUI/NativeRadioButton.h>")!=code->npos&&
            code->find("lvui::native::NativeRadioButton::create(")!=code->npos&&
            code->find("lvui::RadioButtonConfiguration{")!=code->npos&&
            code->find("component.set_options(value)")!=code->npos&&
            code->find("widgets.radio_button(")==code->npos&&code->find("lvrs::")==code->npos,
            "Radio button is not LVUI-owned");
    auto invalid=lvui::compile_native("Window { RadioButton(indicatorSize: -1) }");
    require(!invalid&&invalid.error().line>0,"Invalid static radio size was accepted");
    auto plain=lvui::compile_native("Window { RadioButton(group: \"choice\", state: true, available: false) }");
    require(plain&&plain->find("std::make_shared<lvui::RadioGroups>()")!=plain->npos&&
            plain->find(".checked = true")!=plain->npos&&plain->find(".enabled = false")!=plain->npos,
            "A static radio did not own its group or compatibility inputs");
    require(!lvui::compile_native("Window { RadioButton(checked: true, state: false) }")&&
            !lvui::compile_native("Window { RadioButton(enabled: true, available: false) }"),
            "Conflicting radio aliases were accepted");
}
