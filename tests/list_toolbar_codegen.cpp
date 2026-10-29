#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { ListToolbar(icon1: vm.icon, interactive: vm.interactive, onIconClicked: vm.clicked) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeListToolbar.h>", "lvui::native::NativeListToolbar::create(",
                      "lvui::native::ListToolbarInput", "component.set_options(value)"})
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent list toolbar lost ") + part);
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("List toolbar generated a legacy dependency");
    for (auto source : {"Window { ListToolbar(iconSize: 0) }", "Window { ListToolbar(buttonSize: -1) }",
                        "Window { ListToolbar(interactive: 1) }", "Window { ListToolbar(iconRevision: 1.5) }"})
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid list toolbar configuration was accepted");
}
