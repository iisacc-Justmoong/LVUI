#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { Avatar(id: \"avatar\", size: vm.size, source: vm.image, width: vm.width, backgroundColor: vm.color, accessibilityName: vm.name) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeAvatar.h>","lvui::native::NativeAvatar::create(","lvui::native::AvatarInput","component.set_options(value)"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated Avatar lost ")+token);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos)throw std::runtime_error("Generated Avatar refers to LVRS");
    for(auto source:{"Window { Avatar(size: .bad) }","Window { Avatar(width: -1) }","Window { Avatar(source: \"avatar.png\") }",
                     "Window { Avatar(backgroundColor: \"bad\") }","Window { Avatar { Label() } }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid Avatar input was accepted");
}
