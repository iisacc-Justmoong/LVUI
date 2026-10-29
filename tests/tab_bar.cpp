#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { TabBar(currentIndex: vm.current, autoSelect: vm.automatic, spacing: vm.spacing, onCurrentIndexChanged: vm.changed) { Tab(text: \"A\", iconName: \"home\")\nTab(text: \"B\", iconName: \"generaladd\") } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativeTabBar.h>","lvui::native::NativeTabBar::create(","weak_registration_port()","->finalize()","component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent TabBar lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default TabBar depends on a legacy implementation");
    for(auto source:{"Window { TabBar(currentIndex: 1.5) { Tab(iconName: \"home\") } }",
                     "Window { TabBar { Label() } }","Window { TabBar(spacing: -1) { Tab(iconName: \"home\") } }",
                     "import vm Model\nWindow { TabBar { if (vm.show) { Tab(iconName: \"home\") } } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid TabBar input was accepted");
}
