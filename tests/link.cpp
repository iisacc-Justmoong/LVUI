#include <LVUI/Compiler.h>
#include <stdexcept>
namespace {void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}}
int main(){
    auto code=lvui::compile_native(R"(import vm Model
Window { Link(id: "report", text: vm.text, to: vm.href, params: vm.params, router: vm.router,
              navigator: vm.navigator, targetComponent: vm.page, replace: vm.replace, underline: vm.underline,
              linkColor: vm.color, method: vm.log, methods: [vm.log, vm.after], onClick: vm.clicked,
              onNavigated: vm.navigated) { Label(text: "Custom") } })");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeLink.h>","lvui::native::NativeLink::create(","lvui::LinkConfiguration","lvui::LinkEvent","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated Link lost ")+token);
    require(code->find("lvrs::")==code->npos&&code->find("<LVRS/")==code->npos,"Generated Link refers to LVRS");
    for(auto text:{"Window { Link(href: \"/a\", to: \"/b\") }","Window { Link(linkColor: \"bad\") }",
                   "Window { Link(tone: .bad) }","Window { Link(horizontalPadding: -1) }","Window { Link(params: 1) }"})
        require(!lvui::compile_native(text),"Invalid Link input was accepted");
    auto dynamic=lvui::compile_native("import vm Model\nWindow { Link(methods: vm.methods) }");
    require(dynamic&&dynamic->find("component.set_methods(*value)")!=dynamic->npos&&dynamic->find("lvrs::")==dynamic->npos,
        "Live Link method collection was not generated independently");
}
