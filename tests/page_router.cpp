#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { VStack { Link(href: \"/report\")\n PageRouter(id: \"pages\", router: vm.router, width: vm.width, height: vm.height) } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativePageRouter.h>","lvui::native::NativePageRouter::create(","scope->navigation_context","target.set_extent(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated PageRouter lost ")+token);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos)throw std::runtime_error("Generated PageRouter refers to LVRS");
    for(auto source:{"Window { PageRouter() }","Window { PageRouter(router: 1) }",
            "import vm Model\nWindow { PageRouter(router: vm.router, width: 0) }",
            "import vm Model\nWindow { PageRouter(router: vm.router) { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid PageRouter input was accepted");
}
