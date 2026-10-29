#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { VStack { Label(id: \"anchor\")\nContextMenu(targetId: \"anchor\", selectedIndex: vm.index, itemWidth: vm.width, dismissOnGlobalPress: false, dismissOnGlobalContextRequest: true, onItemTriggered: vm.triggered) { ContextMenuItem(label: vm.label, hasChildItems: false, keepOpen: vm.keep)\nContextMenuDivider(visible: vm.show, thickness: vm.thickness)\nMenuItem(label: \"Second\", hasChildItems: false, closeOnTrigger: true) } } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeMenu.h>","lvui::native::NativeMenu::create(","register_item(","register_divider(","->content_root()","->finalize()","component.set_options(value)","set_context_menu("})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent Menu lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default Menu depends on a legacy implementation");
    auto list=lvui::compile_native("Window { Menu(items: [\"A\", \"B\"]) }");
    if(!list||list->find("NativeMenuItem::create(")==list->npos||list->find("register_item(")==list->npos)
        throw std::runtime_error("Static Menu items did not use independent items");
    auto nested=lvui::compile_native("Window { Menu { MenuItem(label: \"More\") { Menu { MenuItem(label: \"Action\", hasChildItems: false) } } } }");
    if(!nested||nested->find("set_submenu(")==nested->npos||nested->find("weak_popup_port(")==nested->npos)
        throw std::runtime_error("Independent nested Menu was not attached to its item");
    for(auto source:{"Window { Menu(selectedIndex: 1.5) }","Window { Menu(menuOpacity: 2) }",
        "Window { Menu(items: [1]) }","Window { Menu(items: [\"A\"]) { MenuItem() } }",
        "Window { Menu { MenuItem(keepOpen: true, closeOnTrigger: true) } }",
        "Window { MenuItem(hasChildItems: false) { Menu() } }","Window { MenuItem { Menu()\nMenu() } }",
        "import vm Model\nWindow { Menu { if (vm.show) { MenuItem() } } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid or unsupported independent Menu input was accepted");
}
