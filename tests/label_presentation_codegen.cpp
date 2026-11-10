#include <LVUI/Compiler.h>
#include <stdexcept>
int main(){
    auto c=lvui::compile_native("import vm Model\nWindow { Label(text: vm.text, elideRight: vm.elide, alignment: vm.alignment) }");
    if(!c||c->find("set_label_options(value)")==c->npos||c->find("elide_right")==c->npos||c->find("TextAlignment")==c->npos)
        throw std::runtime_error("Reactive label presentation was not generated");
    if(lvui::compile_native("Window { Label(alignment: .invalid) }"))throw std::runtime_error("Invalid label alignment accepted");
    if(lvui::compile_native("Window { Label(elideRight: 3) }"))throw std::runtime_error("Invalid elision accepted");
}
