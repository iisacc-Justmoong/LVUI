#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { MenuItem(label: vm.label, shortcut: vm.shortcut, iconName: vm.icon, iconSource: vm.source, iconRevision: vm.revision, state: vm.state, selectionDirection: vm.direction, onClick: vm.clicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeMenuItem.h>","lvui::native::NativeMenuItem::create(","lvui::native::MenuItemInput","component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent MenuItem lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default MenuItem depends on a legacy implementation");
    for(auto source:{"Window { MenuItem(state: .bad) }","Window { ContextMenuItem(selectionDirection: .bad) }",
                     "Window { MenuItem(key: \"A\", shortcut: \"B\") }","Window { MenuItem(iconSize: 0) }",
                     "Window { MenuItem(keepOpen: true) }","Window { MenuItem { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid or unsupported MenuItem input was accepted");
    auto legacy=lvui::compile_native("Window { MenuItem(label: \"Open\") }",{.framework="LVRS"});
    if(!legacy||legacy->find("controls.menu_item(")==legacy->npos)throw std::runtime_error("Legacy MenuItem generation was altered");
}
