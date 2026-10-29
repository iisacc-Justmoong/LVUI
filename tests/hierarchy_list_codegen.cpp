#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { HierarchyList(controller: vm.controller, editor: vm.editor, editable: true, modelRevision: vm.revision, rowHeight: 20, onActiveChanged: vm.activated, onItemMoved: vm.moved) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeHierarchyList.h>", "lvui::native::NativeHierarchyList::create(",
                      "lvui::native::HierarchyListInput", "component.set_options(value)", "lvui::HierarchyMovePlan"})
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent hierarchy list lost ") + part);
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default hierarchy list depends on a legacy implementation");
    auto empty = lvui::compile_native("Window { HierarchyList() }");
    if (!empty || empty->find("std::make_shared<lvui::HierarchyMemory>()") == empty->npos)
        throw std::runtime_error("An empty independent hierarchy list did not own its default source");
    for (auto source : {"Window { HierarchyList(width: 0) }", "Window { HierarchyList(rowHeight: 17) }",
                        "Window { HierarchyList(overscan: 257) }", "Window { HierarchyList(overscan: 1.5) }",
                        "Window { HierarchyList(rowHeight: 20, generatedRowHeight: 20) }",
                        "Window { HierarchyList(editable: true) }", "Window { HierarchyList(itemDelegate: \"x\") }"})
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid or unsupported independent hierarchy list was accepted");
}
