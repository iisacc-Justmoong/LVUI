#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { DropdownButton(id: \"menu\", text: vm.text, iconMode: vm.iconMode, icon: vm.icon, enabled: vm.enabled, onClick: vm.open) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeDropdownButton.h>","lvui::native::NativeDropdownButton::create(","lvui::native::DropdownInput","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated Dropdown lost ")+token);
    if(code->find("widgets.dropdown_button(")!=code->npos||code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated Dropdown depends on a prebuilt or legacy control");
    for(auto source:{"Window { DropdownButton(width: 0) }","Window { LabelMenuButton(tone: .invalid) }",
                     "Window { DropdownButton { Label() } }",
                     "import vm Model\nWindow { IconMenuButton(icon: vm.icon, iconName: \"home\") }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid Dropdown input was accepted");
    if(!lvui::compile_native("Window { IconMenuButton(iconName: \"projectStructure\") }"))
        throw std::runtime_error("Independent named icon was rejected");
    auto custom=lvui::compile_native("import vm Model\nWindow { IconMenuButton(iconName: vm.name, iconSource: vm.source, iconRevision: vm.revision) }");
    if(!custom||custom->find("value.icon_name = ")==custom->npos||custom->find("value.icon_source = ")==custom->npos||
       custom->find("value.icon_revision = ")==custom->npos)throw std::runtime_error("Dropdown named icon inputs were lost");
    auto legacy=lvui::compile_native("Window { DropdownButton(text: \"Open\") }",{.framework="LVRS"});
    if(!legacy||legacy->find("controls.dropdown_button(")==legacy->npos)throw std::runtime_error("Legacy Dropdown path was altered");
}
