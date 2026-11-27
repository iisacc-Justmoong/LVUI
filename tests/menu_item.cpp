#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { MenuItem(label: vm.label, shortcut: vm.shortcut, iconName: vm.icon, iconSource: vm.source, iconRevision: vm.revision, state: vm.state, selectionDirection: vm.direction, onClick: vm.clicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI>","#include <iiGUI>","lvui::native::MenuItemBinding<","lvui::native::MenuItemInputValue<","component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent MenuItem lost ")+part);
    const auto paletteValue=[&](const char* name) {
        const std::string prefix=std::string("value.configuration.palette.")+name+" = ";
        const auto start=code->find(prefix);
        if(start==code->npos)throw std::runtime_error("Generated menu palette role is missing");
        const auto valueStart=start+prefix.size(),end=code->find(';',valueStart);
        return code->substr(valueStart,end-valueStart);
    };
    if(paletteValue("selected")!=paletteValue("pressed") || paletteValue("selected")==paletteValue("focus"))
        throw std::runtime_error("Generated menu selection must match press, not the primary accent");
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default MenuItem depends on a legacy implementation");
    for(auto source:{"Window { MenuItem(state: .bad) }","Window { ContextMenuItem(selectionDirection: .bad) }",
                     "Window { MenuItem(key: \"A\", shortcut: \"B\") }","Window { MenuItem(iconSize: 0) }",
                     "Window { MenuItem(keepOpen: true) }","Window { MenuItem { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid or unsupported MenuItem input was accepted");
}
