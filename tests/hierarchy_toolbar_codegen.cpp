#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { HierarchyToolbar(buttonItems: vm.items, activeButtonId: vm.active, onActiveChanged: vm.changed, onButtonTriggered: vm.triggered) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeHierarchyToolbar.h>", "lvui::native::NativeHierarchyToolbar::create(",
                      "lvui::native::HierarchyToolbarInput", "lvui::native::hierarchy_toolbar_items(", "lvui::native::hierarchy_toolbar_active(",
                      "component.set_options(value)", "const lvui::HierarchyToolbarSelection& value"})
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent hierarchy toolbar lost ") + part);
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default hierarchy toolbar depends on legacy widgets");
    auto empty = lvui::compile_native("Window { HierarchyToolbar() }");
    if (!empty || empty->find("NativeHierarchyToolbar::create") == empty->npos) throw std::runtime_error("Default hierarchy toolbar was not generated");
    for (auto source : {"Window { HierarchyToolbar(buttonItems: [1]) }", "Window { HierarchyToolbar(activeButtonId: 1.5) }",
                        "Window { HierarchyToolbar(slotSize: -1) }", "Window { HierarchyToolbar { ToolbarButton() } }",
                        "import vm Model\nWindow { HierarchyToolbar(onButtonEventTriggered: vm.event) }"})
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent hierarchy toolbar input was accepted");
}
