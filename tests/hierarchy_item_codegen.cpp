#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { HierarchyItem(label: vm.label, count: vm.count, expanded: vm.open, hasChildItems: true, iconName: \"home\", onClick: vm.clicked, onExpansionChanged: vm.expanded) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeHierarchyItem.h>", "lvui::native::NativeHierarchyItem::create(",
                      "lvui::native::HierarchyItemInput", "component.set_options(value)"})
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent hierarchy item lost ") + part);
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default hierarchy item depends on a legacy implementation");
    for (auto source : {"Window { HierarchyItem(width: 0) }", "Window { HierarchyItem(count: -2) }",
                        "Window { HierarchyItem(indentLevel: -1) }", "Window { HierarchyItem(text: \"a\", label: \"b\") }",
                        "import vm Model\nWindow { HierarchyItem(controller: vm.controller) }"}) {
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid independent hierarchy item was accepted");
    }
}
