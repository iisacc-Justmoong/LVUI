#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { ListItem(type: .Navigation, label: vm.label, description: vm.description, selected: vm.selected, visible: vm.visible, onClick: vm.clicked) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeListItem.h>", "lvui::native::NativeListItem::create(",
                      "lvui::native::ListItemInput", "lvui::ListItemVariant::navigation", "component.set_options(value)"})
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent list item lost ") + part);
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default list item depends on legacy widgets");
    for (auto source : {"Window { ListItem() }", "Window { ListItem(type: .Mini) }", "Window { ListItem(size: .Detail) }"}) {
        auto item = lvui::compile_native(source);
        if (!item || item->find("NativeListItem::create") == item->npos) throw std::runtime_error("List item was not generated");
    }
    for (auto source : {"Window { ListItem(type: .DetailActions) }", "Window { ListItem(type: .unknown) }",
                        "Window { ListItem(width: -1) }", "Window { ListItem(icon: 1, iconName: \"home\") }",
                        "Window { ListItem(label: \"A\", text: \"B\") }", "Window { ListItem { Label(text: \"bad\") } }"})
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent list item input was accepted");
}
