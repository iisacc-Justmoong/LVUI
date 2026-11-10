#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    for(const auto name:{"MobileNavigationBar","MobileTabBar"}){
        const auto source=std::string("import vm Model\nWindow { ")+name+
            "(id: \"tabs\", model: vm.items, currentIndex: vm.current, autoSelect: false, width: vm.width, bottomSafeInset: vm.inset, onActivated: vm.activate, onSearchRequested: vm.search) }";
        auto code=lvui::compile_native(source);if(!code)throw std::runtime_error(code.error().message);
        for(auto token:{"NativeMobileNavigation::create","MobileNavigationConfiguration","component.set_options(value)","NativeMobileNavigation.h"})
            if(code->find(token)==code->npos)throw std::runtime_error(std::string("Mobile navigation codegen lost ")+token);
        if(code->find("<Qt")!=code->npos||code->find("<LVRS/")!=code->npos)
            throw std::runtime_error("Mobile navigation generated a Qt or LVRS dependency");
    }
    if(lvui::compile_native("Window { MobileNavigationBar(width: 0) }"))throw std::runtime_error("Invalid width accepted");
    auto android=lvui::compile_native("Window { MobileNavigationBar(platformStyle: .android) }");
    if(!android||android->find("MobileNavigationPlatform::android")==android->npos)throw std::runtime_error("Android style was not generated");
    auto accent=lvui::compile_native("import vm Model\nWindow(accent: vm.accent) { MobileNavigationBar() }");
    if(!accent||accent->find("value.focus_color")==accent->npos||accent->find("context.read(")==accent->npos||
       accent->find("component.set_options(value)")==accent->npos)
        throw std::runtime_error("Mobile navigation lost its reactive window accent");
}
