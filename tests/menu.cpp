#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { VStack { Label(id: \"anchor\")\nContextMenu(targetId: \"anchor\", selectedIndex: vm.index, itemWidth: vm.width, dismissOnGlobalPress: false, dismissOnGlobalContextRequest: true, onItemTriggered: vm.triggered) { ContextMenuItem(label: vm.label, hasChildItems: false, keepOpen: vm.keep)\nContextMenuDivider(visible: vm.show, thickness: vm.thickness)\nMenuItem(label: \"Second\", hasChildItems: false, closeOnTrigger: true) } } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeMenu.h>","lvui_generated::detail_menu::NativeMenu::create(","weak_registration_port(","divider_port(","->content_root()","->finalize()","component.set_options(value)","set_context_menu("})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent Menu lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default Menu depends on a legacy implementation");
    auto list=lvui::compile_native("Window { Menu(items: [\"A\", \"B\"]) }");
    if(!list||list->find("MenuItemBinding<")==list->npos||list->find("register_item(")==list->npos)
        throw std::runtime_error("Static Menu items did not use independent items");
    auto nested=lvui::compile_native("Window { Menu { MenuItem(label: \"More\") { Menu { MenuItem(label: \"Action\", hasChildItems: false) } } } }");
    if(!nested||nested->find("set_submenu(")==nested->npos||nested->find("weak_popup_port(")==nested->npos)
        throw std::runtime_error("Independent nested Menu was not attached to its item");
    auto conditional=lvui::compile_native("import vm Model\nWindow { Menu { if (vm.a) { MenuItem() } else if (vm.b) { MenuDivider() } else { MenuItem() } } }");
    if(!conditional||conditional->find("register_section(")==conditional->npos||
       conditional->find("capture_menu_item")==conditional->npos||conditional->find("divider_port(")==conditional->npos||
       conditional->find("target.set_content(std::move(factory)")==conditional->npos)
        throw std::runtime_error("Conditional Menu did not use independent section publication");
    if(conditional->find("lvrs::")!=conditional->npos||conditional->find("<LVRS/")!=conditional->npos||conditional->find("<Qt")!=conditional->npos)
        throw std::runtime_error("Conditional Menu introduced a legacy dependency");
    for(auto kind:{"MenuItem", "ContextMenuItem", "MenuDivider", "ContextMenuDivider"}){
        const auto source=std::string("import vm Model\nWindow { Menu { for (item in vm.items, key: item.id) { ")+kind+"() } } }";
        auto repeated=lvui::compile_native(source);
        if(!repeated||repeated->find("register_section(")==repeated->npos||repeated->find("set_keyed_content(")==repeated->npos)
            throw std::runtime_error("Repeated Menu did not publish keyed native entries");
        if(repeated->find("lvrs::")!=repeated->npos||repeated->find("<LVRS/")!=repeated->npos||repeated->find("<Qt")!=repeated->npos)
            throw std::runtime_error("Repeated Menu introduced a legacy dependency");
    }
    if(!lvui::compile_native("import vm Model\nWindow { Menu { for (item in vm.items, key: item.id) { if (item.show) { MenuItem() } else { MenuDivider() } } } }"))
        throw std::runtime_error("A conditional repeated menu row did not compile");
    auto sequence=lvui::compile_native("let Actions(): [View] = [MenuItem(id: \"one\"), MenuDivider(id: \"separator\"), MenuItem(id: \"two\")]\nWindow { Menu { Actions() } }");
    if(!sequence||sequence->find("divider_port(")==sequence->npos||sequence->find("MenuItemBinding<")==sequence->npos)
        throw std::runtime_error("A direct multi-item menu sequence did not compile");
    auto dynamic_sequence=lvui::compile_native("import vm Model\nlet Actions(title: String): [View] = [MenuItem(label: title), if (vm.show) { MenuDivider() }, for (item in vm.items, key: item.id) { MenuItem(label: item.label) }, MenuItem(label: title)]\nWindow { Menu(itemSpacing: vm.spacing) { Actions(title: vm.title) } }");
    if(!dynamic_sequence||dynamic_sequence->find("register_section(")==dynamic_sequence->npos||dynamic_sequence->find("set_keyed_content(")==dynamic_sequence->npos)
        throw std::runtime_error("A direct menu sequence did not compile its conditional and repeated children");
    auto conditional_sequence=lvui::compile_native("import vm Model\nlet Actions(): [View] = [MenuItem(), MenuDivider(), MenuItem()]\nWindow { Menu { if (vm.show) { Actions() } } }");
    if(!conditional_sequence||conditional_sequence->find("register_section(")==conditional_sequence->npos)
        throw std::runtime_error("A conditional multi-item menu sequence did not compile");
    auto repeated_sequence=lvui::compile_native("import vm Model\nlet Actions(): [View] = [MenuItem(), MenuDivider(), MenuItem()]\nWindow { Menu { for (item in vm.items, key: item.id) { Actions() } } }");
    if(!repeated_sequence||repeated_sequence->find("set_keyed_content(")==repeated_sequence->npos)
        throw std::runtime_error("A repeated multi-item menu sequence did not compile");
    auto nested_sequence=lvui::compile_native("import vm Model\nlet Actions(): [View] = [MenuItem(), if (vm.show) { MenuDivider() }, for (child in vm.items, key: child.id) { MenuItem() }]\nWindow { Menu { if (vm.show) { Actions() }\nfor (item in vm.items, key: item.id) { Actions() } } }");
    if(!nested_sequence||nested_sequence->find("->slot()")==nested_sequence->npos||nested_sequence->find("->publish(")==nested_sequence->npos||nested_sequence->find("lvrs::")!=nested_sequence->npos)
        throw std::runtime_error("Nested menu composition did not generate independent staged slots");
    if(!lvui::compile_native("import vm Model\nWindow { Menu { for (parent in vm.items, key: parent.id) { for (child in vm.items, key: child.id) { MenuItem() } } } }"))
        throw std::runtime_error("Nested keyed Menu repetitions did not compile");
    for(auto source:{"Window { Menu(selectedIndex: 1.5) }","Window { Menu(menuOpacity: 2) }",
        "Window { Menu(items: [1]) }","Window { Menu(items: [\"A\"]) { MenuItem() } }",
        "Window { Menu { MenuItem(keepOpen: true, closeOnTrigger: true) } }",
        "Window { MenuItem(hasChildItems: false) { Menu() } }","Window { MenuItem { Menu()\nMenu() } }",
        "import vm Model\nWindow { Menu { if (vm.show) { Label() } } }",
        "import vm Model\nWindow { Menu { if (vm.show) { MenuItem()\nMenuItem() } } }",
        "import vm Model\nWindow { Menu { for (item in vm.items, key: item.id) { Label() } } }",
        "import vm Model\nWindow { Menu { for (item in vm.items, key: item.id, spacing: 3) { MenuItem() } } }",
        "import vm Model\nlet Bad(): View = Label()\nWindow { Menu { for (item in vm.items, key: item.id) { Bad() } } }",
        "let Bad(): View = Label()\nWindow { Menu { Bad() } }",
        "let Bad(): [View] = [MenuItem(), Label()]\nWindow { Menu { Bad() } }",
        "import vm Model\nlet Bad(): [View] = [MenuItem(), if (vm.show) { Label() }]\nWindow { Menu { Bad() } }",
        "import vm Model\nlet Bad(): [View] = [MenuItem(), for (item in vm.items, key: item.id) { Label() }]\nWindow { Menu { Bad() } }",
        "import vm Model\nlet Bad(): [View] = [MenuItem(), Label()]\nWindow { Menu { if (vm.show) { Bad() } } }",
        "import vm Model\nlet Bad(): [View] = [MenuItem(), Label()]\nWindow { Menu { for (item in vm.items, key: item.id) { Bad() } } }",
        "import vm Model\nlet Bad(): [View] = [MenuItem(), if (vm.show) { Label() }]\nWindow { Menu { if (vm.show) { Bad() } } }",
        "import vm Model\nlet Bad(): [View] = [MenuItem(), for (child in vm.items, key: child.id) { Label() }]\nWindow { Menu { for (item in vm.items, key: item.id) { Bad() } } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid or unsupported independent Menu input was accepted");
}
