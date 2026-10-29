#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { AppHeader(id: \"header\", title: vm.title, subtitle: vm.subtitle, compact: vm.compact, menuVisible: vm.menu_visible, onMenuClicked: vm.menu) { Label(text: \"Action\") } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeAppHeader.h>","lvui::native::NativeAppHeader::create(","lvui::AppHeaderConfiguration","component.set_options(value)","->actions_root()"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated AppHeader lost ")+token);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos)throw std::runtime_error("Generated AppHeader refers to LVRS");
    for(auto source:{"Window { AppHeader(compact: 1) }","Window { AppHeader(rowSpacing: -1) }","Window { AppHeader(menuButtonPadding: 4091) }","Window { AppHeader(backgroundColor: \"bad\") }","Window { AppHeader(onMenuClicked: 1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid AppHeader input was accepted");
}
