#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main() {
    auto code=lvui::compile_native("import vm Model\nWindow { Hierarchy(controller: vm.controller, editor: vm.editor, editable: true, modelRevision: vm.revision, toolbarItems: vm.tools, activeToolbarButtonId: vm.active, footerVisible: vm.footer, width: vm.width, onItemActivated: vm.activated, onToolbarTriggered: vm.tool, onFooterClicked: vm.footerClicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeHierarchyPanel.h>","lvui::native::NativeHierarchyPanel::create(",
                   "lvui::native::HierarchyPanelInput","lvui::native::hierarchy_toolbar_items(",
                   "component.set_options(value)","const lvui::native::HierarchyListEvent& event",
                   "const lvui::HierarchyToolbarSelection& value","const lvui::ListFooterSlot& slot"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent hierarchy panel lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default hierarchy panel depends on legacy widgets");
    auto empty=lvui::compile_native("Window { Hierarchy() }");
    if(!empty||empty->find("NativeHierarchyPanel::create")==empty->npos)throw std::runtime_error("Empty hierarchy panel was not generated");
    for(auto source:{"Window { Hierarchy(editable: true) }","Window { Hierarchy(width: 0) }",
                     "Window { Hierarchy(toolbarItems: [1]) }","Window { Hierarchy { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid independent hierarchy panel input was accepted");
}
