#include <LVUI/Compiler.h>
#include <stdexcept>
int main(){
    auto code=lvui::compile_native("Window { AbstractButton(accessibilityName: \"Account\") { Avatar() } }");
    if(!code||code->find("lvui::native::ButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton")==code->npos||
       code->find("NativeAvatar::create(widgets, *node_0")==code->npos)
        throw std::runtime_error("Composed account button was not generated");
    if(lvui::compile_native("Window { AbstractButton(text: \"Account\") { Avatar() } }"))
        throw std::runtime_error("Conflicting authored content was accepted");
    if(lvui::compile_native("Window { AbstractButton() { Label() Avatar() } }"))
        throw std::runtime_error("Multiple direct content views were accepted");
}
