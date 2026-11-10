#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    for(const char* name:{"MobileTab","MobileNavigationTab"}){
        const std::string source="import vm Model\nWindow { "+std::string(name)+
            "(iconName: \"home\", text: vm.label, selected: vm.selected, badge: vm.badge, badgeDot: false) }";
        auto code=lvui::compile_native(source);if(!code)throw std::runtime_error(code.error().message);
        for(auto part:{"#include <iiGUI/NativeMobileTabView.h>","detail_mobile_tab::NativeMobileTab::create(","component.set_options(value)"})
            if(code->find(part)==code->npos)throw std::runtime_error(std::string("Mobile tab lost iiGUI support: ")+part);
        if(lvui::compile_native("Window { "+std::string(name)+"(text: \"Missing icon\") }"))
            throw std::runtime_error("Mobile tab accepted a missing icon");
    }
}
