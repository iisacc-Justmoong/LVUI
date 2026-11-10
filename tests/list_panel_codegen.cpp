#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { List(controller: vm.controller, editor: vm.editor, editable: vm.editable, reorderable: vm.reorderable, scrollable: vm.scrollable, modelRevision: vm.revision, viewRevision: vm.viewRevision, width: vm.width, toolbarVisible: true, toolbarIcon1: \"home\", footerVisible: vm.footer, onItemActivated: vm.selected, onItemEdited: vm.edited, onItemSubmitted: vm.submitted, onItemRejected: vm.rejected, onItemMoved: vm.moved, onToolbarIconTriggered: vm.tool, onFooterButtonTriggered: vm.footerClicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeListPanel.h>","lvui_generated::detail_list_panel::NativeListPanel::create(","lvui::native::ListPanelInput",
                   "value.list.view_revision","value.list.editor","value.list.editable","value.list.reorderable","value.list.scrollable","component.set_options(value)",
                   "const lvui::native::ListViewEvent& event","const lvui::native::ListEditEvent& event","const lvui::ListDragProposal& event","const lvui::ListFooterSlot& slot"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent list panel lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default list panel depends on legacy widgets");
    auto empty=lvui::compile_native("Window { List() }");
    if(!empty||empty->find("lvui::ListMemory::create()") == empty->npos)throw std::runtime_error("Default list did not create its own model");
    auto source=lvui::compile_native("import vm Model\nWindow { List(model: vm.source) }");
    if(!source||source->find("lvui::ListController::create(")==source->npos)throw std::runtime_error("List source did not receive an owned view controller");
    for(const auto input:{"Window { List(itemRow: \"item\", itemDelegate: Label(text: item.entry.label)) }",
                          "let Rows(content: View): View = List(itemRow: \"item\", itemDelegate: content)\nWindow { Rows(content: Label(text: item.entry.label)) }",
                          "Window { List(itemDelegate: Label(text: \"Custom\")) }",
                          "import item Model\nWindow { VStack {\n List(itemRow: \"item\", itemDelegate: Label(text: item.entry.label))\n Label(text: item.badge)\n} }",
                          "Window { List(itemRow: \"outer\", itemDelegate: List(itemRow: \"inner\", itemDelegate: Label(text: outer.entry.label))) }"}){
        auto delegate=lvui::compile_native(input);
        if(!delegate||delegate->find("lvui::native::ListRowViewFactory")==delegate->npos||delegate->find("lvrs::")!=delegate->npos)
            throw std::runtime_error(delegate?"List row delegate did not use the owned factory":delegate.error().message);
    }
    for(auto input:{"import vm Model\nWindow { List(model: vm.source, controller: vm.controller) }",
                    "Window { List(width: 0) }","Window { List(rowHeight: 1) }","Window { List { Label() } }",
                    "Window { List(editable: true) }","Window { List(reorderable: true) }","Window { List(scrollable: 1) }","Window { List(itemRow: \"item\") }",
                    "Window { List(itemRow: \"bad-name\", itemDelegate: Label()) }",
                    "Window { List(itemRow: 3, itemDelegate: Label()) }","Window { List(viewRevision: -1) }","Window { List(viewRevision: 1.5) }",
                    "Window { List(itemDelegate: [Label(), Label()]) }"})
        if(lvui::compile_native(input))throw std::runtime_error("Invalid independent list input was accepted");
}
