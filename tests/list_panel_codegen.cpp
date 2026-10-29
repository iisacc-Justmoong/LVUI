#include <LVUI/Compiler.h>

#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { List(controller: vm.controller, modelRevision: vm.revision, width: vm.width, toolbarVisible: true, toolbarIcon1: \"home\", footerVisible: vm.footer, onItemActivated: vm.selected, onToolbarIconTriggered: vm.tool, onFooterButtonTriggered: vm.footerClicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeListPanel.h>","lvui::native::NativeListPanel::create(","lvui::native::ListPanelInput",
                   "component.set_options(value)","const lvui::native::ListViewEvent& event","const lvui::ListFooterSlot& slot"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent list panel lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default list panel depends on legacy widgets");
    auto empty=lvui::compile_native("Window { List() }");
    if(!empty||empty->find("lvui::ListMemory::create()") == empty->npos)throw std::runtime_error("Default list did not create its own model");
    auto source=lvui::compile_native("import vm Model\nWindow { List(model: vm.source) }");
    if(!source||source->find("lvui::ListController::create(")==source->npos)throw std::runtime_error("List source did not receive an owned view controller");
    for(auto input:{"import vm Model\nWindow { List(model: vm.source, controller: vm.controller) }",
                    "Window { List(width: 0) }","Window { List(rowHeight: 1) }","Window { List { Label() } }",
                    "Window { List(editable: true) }"})
        if(lvui::compile_native(input))throw std::runtime_error("Invalid independent list input was accepted");
}
