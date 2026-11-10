#include <LVUI/Compiler.h>
#include <stdexcept>
int main(){auto code=lvui::compile_native("import vm Model\nWindow { Card(type: .File, filename: vm.name, onClick: vm.select, onDoubleClicked: vm.open) }");
    if(!code)throw std::runtime_error(code.error().message);
    if(code->find(".double_clicked") == code->npos)throw std::runtime_error("Double activation callback missing");
    if(lvui::compile_native("import vm Model\nWindow { Card(onDoubleClick: vm.open, onDoubleClicked: vm.open) }"))
        throw std::runtime_error("Conflicting double activation callbacks accepted");}
