#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { Popover(width: vm.width, surfaceColor: vm.color, closeOnContextRequest: false, onOpened: vm.opened, onCanceled: vm.canceled) { Label(text: \"Body\") } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <LVUI/NativePopover.h>","iiGUI/NativePopoverView.h","lvui::native::PopoverBinding<","lvui::PopoverConfiguration","component.set_options(value)"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Independent Popover lost ")+part);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Default Popover depends on a legacy implementation");
    auto fallback=lvui::compile_native("Window { Popover(blurEnabled: false) }");
    if(!fallback||fallback->find("value.blur_enabled = false") == fallback->npos)
        throw std::runtime_error("Popover blur fallback was not generated");
    auto still=lvui::compile_native("Window { Popover(motionEnabled: false) }");
    if(!still||still->find("value.motion_enabled = false") == still->npos)
        throw std::runtime_error("Static Popover motion setting was not generated");
    auto markup=lvui::compile_native("Window { Popover { motionEnabled: false } }");
    if(!markup||markup->find("value.motion_enabled = false") == markup->npos)
        throw std::runtime_error("Body-declared Popover motion setting was not generated");
    auto reactive=lvui::compile_native("import vm Model\nWindow { Popover(motionEnabled: vm.motion) }");
    if(!reactive||reactive->find("value.motion_enabled = lvui::native::truth(")==reactive->npos||
        reactive->find("component.set_options(value)")==reactive->npos)
        throw std::runtime_error("Reactive Popover motion setting was not generated");
    for(auto source:{"Window { Popover(width: -1) }","Window { Popover(surfaceOpacity: 2) }",
        "Window { Popover(surfaceColor: \"bad\") }","Window { Popover(blurEnabled: 1) }","Window { Popover(motionEnabled: 1) }","Window { Popover { Label()\nLabel() } }",
        "import vm Model\nWindow { Popover(onOpened: 1) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid Popover input was accepted");
}
