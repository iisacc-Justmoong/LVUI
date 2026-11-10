#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main() {
    auto code=lvui::compile_native("import vm Model\nWindow { Hierarchy(controller: vm.controller, editor: vm.editor, editable: true, modelRevision: vm.revision, viewRevision: vm.viewRevision, toolbarItems: vm.tools, activeToolbarButtonId: vm.active, footerVisible: vm.footer, width: vm.width, onItemActivated: vm.activated, onToolbarTriggered: vm.tool, onFooterClicked: vm.footerClicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeHierarchyPanel.h>","lvui_generated::detail_hierarchy_panel::NativeHierarchyPanel::create(",
                   "lvui::native::HierarchyPanelInput","value.view_revision","lvui::native::hierarchy_toolbar_items(",
                   "component.set_options(value)","const lvui::native::HierarchyListEvent& event",
                   "const lvui::HierarchyToolbarSelection& value","const lvui::ListFooterSlot& slot"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent hierarchy panel lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default hierarchy panel depends on legacy widgets");
    auto empty=lvui::compile_native("Window { Hierarchy() }");
    if(!empty||empty->find("NativeHierarchyPanel::create")==empty->npos)throw std::runtime_error("Empty hierarchy panel was not generated");
    for(auto source:{"let Badge(text: String): View = Label(text: text)\nWindow { Hierarchy(countView: Badge(text: \"Count\")) }",
                     "let Badge: View = Label(text: \"Count\")\nlet Panel(content: View): View = Hierarchy(countView: content)\nWindow { Panel(content: Badge) }",
                     "Window { Hierarchy(countView: Label(text: \"Count\")) }"}) {
        auto count=lvui::compile_native(source);
        if(!count||count->find("HierarchyCountViewFactory")==count->npos)throw std::runtime_error("Hierarchy panel did not compile its count View");
    }
    auto row_count=lvui::compile_native("Window { Hierarchy(countRow: \"item\", countView: Label(text: item.entry.count)) }");
    if(!row_count||row_count->find("hierarchy_view_row")==row_count->npos)throw std::runtime_error("Hierarchy panel did not bind its count View to the row context");
    auto delegate=lvui::compile_native("Window { Hierarchy(itemRow: \"item\", itemDelegate: Label(text: item.entry.label)) }");
    if(!delegate||delegate->find("HierarchyRowViewFactory")==delegate->npos)throw std::runtime_error("Hierarchy panel did not compile its row delegate");
    for(auto source:{"Window { Hierarchy(editable: true) }","Window { Hierarchy(width: 0) }",
                     "Window { Hierarchy(viewRevision: -1) }","Window { Hierarchy(viewRevision: 1.5) }",
                     "Window { Hierarchy(toolbarItems: [1]) }","Window { Hierarchy { Label() } }","Window { Hierarchy(countView: 42) }",
                     "Window { Hierarchy(countView: [Label(), Label()]) }","Window { Hierarchy(countView: Missing) }",
                     "Window { Hierarchy(countRow: \"item\") }","Window { Hierarchy(countRow: \"bad-name\", countView: Label()) }",
                     "Window { Hierarchy(itemRow: \"item\") }","Window { Hierarchy(itemDelegate: Label(), countView: Label()) }",
                     "let Badge(title: String): View = Label(text: title)\nWindow { Hierarchy(countView: Badge) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid independent hierarchy panel input was accepted");
}
