#include <LVUI/Compiler.h>
#include <stdexcept>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { ListItem(type: .Navigation, label: vm.label, value: vm.value, showValue: vm.show, showTrailingIcon: vm.trailing) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto word:{"configuration.value", "configuration.show_value", "configuration.show_chevron", "component.set_options(value)"})
        if(code->find(word)==code->npos)throw std::runtime_error("Navigation value was not generated");
    if(lvui::compile_native("Window { ListItem(showChevron: true, showTrailingIcon: false) }"))
        throw std::runtime_error("Conflicting disclosure flags accepted");
}
