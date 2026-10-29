#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { EventListener(id: \"listener\", width: vm.width, height: vm.height, onInput: vm.input, onResized: vm.resized) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeEventListener.h>","lvui::native::NativeEventListener::create(","lvui::EventListenerConfiguration","component.set_options(value)","ii::gui::WidgetInput"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated listener lost ")+token);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos)throw std::runtime_error("Generated listener refers to LVRS");
    for(auto source:{"Window { EventListener(width: 0) }","Window { EventListener(height: 9000) }","Window { EventListener(onInput: 1) }",
            "Window { EventListener() { Label() } }"})if(lvui::compile_native(source))throw std::runtime_error("Invalid listener input was accepted");
}
