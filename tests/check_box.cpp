#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { CheckBox(text: vm.text, checked: vm.checked, enabled: vm.enabled, boxSize: vm.size, checkedColor: vm.color, onToggled: vm.changed) }");
    require(bool(code),"Independent checkbox did not compile");
    require(code->find("#include <LVUI/NativeCheckBox.h>")!=code->npos&&
            code->find("lvui::native::NativeCheckBox::create(")!=code->npos&&
            code->find("lvui::CheckBoxConfiguration{")!=code->npos&&
            code->find("component.set_options(value)")!=code->npos&&
            code->find("widgets.check_box(")==code->npos&&code->find("lvrs::")==code->npos,
            "Checkbox is not LVUI-owned");
    auto invalid=lvui::compile_native("Window { CheckBox(boxSize: -1) }");
    require(!invalid&&invalid.error().line>0,"Invalid static checkbox size was accepted");
}
