#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { AppCard(id: \"card\", title: vm.title, subtitle: vm.subtitle, width: vm.width, shapeStyle: vm.shape, backgroundColor: vm.color) { Label(text: \"Body\") } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeAppCard.h>","lvui::native::NativeAppCard::create(","lvui::AppCardConfiguration","component.set_options(value)","->content_root()"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated AppCard lost ")+token);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos)throw std::runtime_error("Generated AppCard refers to LVRS");
    for(auto source:{"Window { AppCard(shapeStyle: .bad) }","Window { AppCard(padding: -1) }","Window { AppCard(backgroundColor: \"bad\") }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid AppCard input was accepted");
}
