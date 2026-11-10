#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main() {
    auto code = lvui::compile_native("import vm Model\nWindow { HierarchyList(controller: vm.controller, editor: vm.editor, editable: true, modelRevision: vm.revision, viewRevision: vm.viewRevision, rowHeight: 20, onActiveChanged: vm.activated, onItemMoved: vm.moved) }");
    if (!code) throw std::runtime_error(code.error().message);
    for (auto part : {"#include <LVUI/NativeHierarchyList.h>", "lvui_generated::detail_hierarchy_list::NativeHierarchyList::create(",
                      "lvui::native::HierarchyListInput", "value.view_revision", "component.set_options(value)", "lvui::HierarchyMovePlan"})
        if (code->find(part) == code->npos) throw std::runtime_error(std::string("Independent hierarchy list lost ") + part);
    if (code->find("lvrs::") != code->npos || code->find("<LVRS/") != code->npos || code->find("<Qt") != code->npos)
        throw std::runtime_error("Default hierarchy list depends on a legacy implementation");
    auto empty = lvui::compile_native("Window { HierarchyList() }");
    if (!empty || empty->find("std::make_shared<lvui::HierarchyMemory>()") == empty->npos)
        throw std::runtime_error("An empty independent hierarchy list did not own its default source");
    for(auto source:{"let Badge(text: String): View = Label(text: text)\nWindow { HierarchyList(countView: Badge(text: \"Count\")) }",
                     "let Badge: View = Label(text: \"Count\")\nlet Rows(content: View): View = HierarchyList(countView: content)\nWindow { Rows(content: Badge) }",
                     "Window { HierarchyList(countView: Label(text: \"Count\")) }"}) {
        auto count = lvui::compile_native(source);
        if (!count || count->find("HierarchyCountViewFactory") == count->npos)
            throw std::runtime_error("HierarchyList did not compile a custom count View");
    }
    auto row_count=lvui::compile_native("Window { HierarchyList(countRow: \"item\", countView: Label(text: item.entry.count)) }");
    if(!row_count||row_count->find("hierarchy_view_row")==row_count->npos)
        throw std::runtime_error("HierarchyList did not bind its count View to the row context");
    for (auto source : {
            "import item Model\nWindow { VStack { HierarchyList(countRow: \"item\", countView: Label(text: item.entry.count))\n Label(text: item.badge) } }",
            "let Rows(content: View): View = HierarchyList(countRow: \"item\", countView: content)\nWindow { Rows(content: Label(text: item.entry.count)) }"}) {
        auto scoped=lvui::compile_native(source);
        if(!scoped||scoped->find("hierarchy_view_row")==scoped->npos)
            throw std::runtime_error(scoped?"A scoped hierarchy row was not generated":scoped.error().message);
    }
    for (auto source : {
            "Window { HierarchyList(itemRow: \"item\", itemDelegate: Label(text: item.entry.label)) }",
            "let Rows(content: View): View = HierarchyList(itemRow: \"item\", itemDelegate: content)\nWindow { Rows(content: Label(text: item.entry.label)) }",
            "import item Model\nWindow { VStack { HierarchyList(itemRow: \"item\", itemDelegate: Label(text: item.entry.label))\n Label(text: item.badge) } }",
            "Window { HierarchyList(itemDelegate: Label(text: \"Row\")) }"}) {
        auto custom=lvui::compile_native(source);
        if(!custom||custom->find("HierarchyRowViewFactory")==custom->npos)
            throw std::runtime_error(custom?"A hierarchy row delegate was not generated":custom.error().message);
    }
    for (auto source : {"Window { HierarchyList(width: 0) }", "Window { HierarchyList(rowHeight: 17) }",
                        "Window { HierarchyList(viewRevision: -1) }", "Window { HierarchyList(viewRevision: 1.5) }",
                        "Window { HierarchyList(overscan: 257) }", "Window { HierarchyList(overscan: 1.5) }",
                        "Window { HierarchyList(rowHeight: 20, generatedRowHeight: 20) }",
                        "Window { HierarchyList(editable: true) }", "Window { HierarchyList(itemDelegate: \"x\") }",
                        "Window { HierarchyList(countView: 42) }", "Window { HierarchyList(countView: [Label(), Label()]) }",
                        "Window { HierarchyList(countView: Missing) }",
                        "Window { HierarchyList(countRow: \"item\") }", "Window { HierarchyList(countRow: \"bad-name\", countView: Label()) }",
                        "Window { HierarchyList(countRow: 4, countView: Label()) }",
                        "Window { HierarchyList(itemRow: \"item\") }", "Window { HierarchyList(itemRow: \"bad-name\", itemDelegate: Label()) }",
                        "Window { HierarchyList(itemRow: 3, itemDelegate: Label()) }",
                        "Window { HierarchyList(itemDelegate: Label(), countView: Label()) }",
                        "Window { VStack { HierarchyList(itemRow: \"item\", itemDelegate: Label(text: item.entry.label))\n Label(text: item.entry.label) } }",
                        "Window { VStack { HierarchyList(countRow: \"item\", countView: Label(text: item.entry.count))\n Label(text: item.entry.count) } }",
                        "let Badge(title: String): View = Label(text: title)\nWindow { HierarchyList(countView: Badge) }"})
        if (lvui::compile_native(source)) throw std::runtime_error("Invalid or unsupported independent hierarchy list was accepted");
}
