#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("Window { WheelScrollGuard(targetId: \"scroll\", width: 100, height: 80) { ScrollView(id: \"scroll\") { VStack { Label(text: \"A\") } } } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto part:{"#include <iiGUI/NativeWheelScrollGuard.h>","ii::gui::NativeWheelScrollGuard::create(","->set_target(*"})
        if(code->find(part)==code->npos)throw std::runtime_error(std::string("Wheel guard lost its iiGUI implementation: ")+part);
    if(lvui::compile_native("Window { WheelScrollGuard() }"))throw std::runtime_error("Wheel guard accepted no scroll target");
    if(lvui::compile_native("Window { WheelScrollGuard(targetId: \"missing\") }"))throw std::runtime_error("Wheel guard accepted an unknown target");
}
