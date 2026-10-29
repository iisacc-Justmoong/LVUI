#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ComboBox(id: \"combo\", text: vm.text, arrow: vm.arrow, tone: vm.tone, enabled: vm.enabled, width: vm.width, onClick: vm.clicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeComboBox.h>","lvui::native::NativeComboBox::create(","lvui::ComboBoxConfiguration","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated ComboBox lost ")+token);
    if(code->find("widgets.combo_box(")!=code->npos||code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated ComboBox depends on a prebuilt or legacy control");
    for(auto source:{"Window { ComboBox(arrow: .Sideways) }","Window { ComboBox(tone: .destructive) }",
                     "Window { ComboBox(width: 18) }","Window { ComboBox(enabled: 1) }","Window { ComboBox { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid ComboBox input was accepted");
    auto legacy=lvui::compile_native("Window { ComboBox(arrow: .up) }",{.framework="LVRS"});
    if(!legacy||legacy->find("controls.combo_box(")==legacy->npos)throw std::runtime_error("Explicit legacy ComboBox path was altered");
}
