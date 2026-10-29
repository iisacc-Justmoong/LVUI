#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { Tab(id: \"tab\", text: vm.text, iconName: vm.name, badge: vm.badge, selected: vm.selected, tabStyle: vm.style, iconSource: vm.source, iconRevision: vm.revision, onClick: vm.clicked) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeTab.h>","lvui::native::NativeTab::create(","lvui::native::TabInput","component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent Tab lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default Tab depends on a legacy implementation");
    for(auto source:{"Window { Tab() }","Window { Tab(iconName: \"  \") }","Window { Tab(iconName: \"home\", tabStyle: .invalid) }",
                     "Window { Tab(iconName: \"home\", iconRevision: 1.5) }","Window { Tab(iconName: \"home\") { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid Tab input was accepted");
}
