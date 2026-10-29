#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { Popover(width: vm.width, surfaceColor: vm.color, closeOnContextRequest: false, onOpened: vm.opened, onCanceled: vm.canceled) { Label(text: \"Body\") } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativePopover.h>","lvui::native::NativePopover::create(","lvui::PopoverConfiguration","component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent Popover lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default Popover depends on a legacy implementation");
    for(auto source:{"Window { Popover(width: -1) }","Window { Popover(surfaceOpacity: 2) }",
        "Window { Popover(surfaceColor: \"bad\") }","Window { Popover { Label()\nLabel() } }",
        "import vm Model\nWindow { Popover(onOpened: 1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid Popover input was accepted");
}
