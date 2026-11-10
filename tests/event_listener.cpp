#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { EventListener(id: \"listener\", width: vm.width, height: vm.height, onInput: vm.input, onResized: vm.resized) }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <iiGUI/NativeEventListenerView.h>","ii::gui::NativeEventListener::create(","ii::gui::EventListenerOptions","component.set_options(value)","ii::gui::WidgetInput"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated listener lost ")+token);
    if(code->find("<LVUI/NativeEventListener.h>")!=code->npos||code->find("lvui::native::NativeEventListener")!=code->npos)
        throw std::runtime_error("Generated listener still uses the LVUI implementation");
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos)throw std::runtime_error("Generated listener refers to LVRS");
    if(!lvui::compile_native("Window { EventListener(width: 0) }"))throw std::runtime_error("Automatic listener width was rejected");
    auto child=lvui::compile_native("Window { EventListener() { Avatar(size: .Small) } }");
    if(!child||child->find("ii::gui::NativeAvatar::create(")==std::string::npos)
        throw std::runtime_error("Listener child was not generated");
    auto hug=lvui::compile_native("Window { EventListener(sizeToContent: true) { Avatar(size: .Large) } }");
    if(!hug||hug->find(".size_to_content = true")==std::string::npos||
       hug->find(".width = 0")==std::string::npos||hug->find(".height = 0")==std::string::npos)
        throw std::runtime_error("Content-sized listener did not generate intrinsic axes");
    auto fixed=lvui::compile_native("Window { EventListener(sizeToContent: true, width: 100) { Avatar() } }");
    if(!fixed||fixed->find(".width = 100")==std::string::npos||fixed->find(".height = 0")==std::string::npos)
        throw std::runtime_error("Content-sized listener lost its explicit axis");
    auto reactive=lvui::compile_native("import vm Model\nWindow { EventListener(sizeToContent: true, width: vm.width) { Avatar() } }");
    if(!reactive||reactive->find(".size_to_content = true")==std::string::npos||
       reactive->find("component.set_options(value)")==std::string::npos)
        throw std::runtime_error("Content-sized listener lost its reactive axis binding");
    for(auto source:{"Window { EventListener(width: -1) }","Window { EventListener(height: 9000) }","Window { EventListener(onInput: 1) }",
            "Window { EventListener() { Label() Label() } }", "import vm Model\nWindow { EventListener(sizeToContent: vm.flag) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Invalid listener input was accepted");
}
