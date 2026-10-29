#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
}
int main(){
    const auto code=lvui::compile_native("import vm Model\nWindow { VStack { MenuDivider(axis: vm.axis, lineLength: vm.length, thickness: vm.thickness, dividerColor: vm.color)\nContextMenuDivider() } }");
    require(bool(code),"Independent divider did not compile");
    require(code->find("#include <LVUI/NativeDivider.h>")!=code->npos&&code->find("lvui::native::NativeDivider::create(")!=code->npos&&
            code->find("lvui::DividerConfiguration{")!=code->npos&&code->find("component.set_options(value)")!=code->npos&&
            code->find("lvrs::")==code->npos&&code->find("widgets.menu_divider(")==code->npos,
            "Divider still depends on legacy implementation");
    const auto invalid=lvui::compile_native("Window { MenuDivider(axis: .diagonal) }");
    require(!invalid&&invalid.error().line>0,"Invalid divider axis was not diagnosed");
}
