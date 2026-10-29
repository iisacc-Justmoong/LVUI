#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

int main(){
    auto code=lvui::compile_native("import vm Model\nWindow { VStack {\n Tooltip(id: \"hint\", targetId: \"anchor\", text: vm.text, delay: 0, timeout: 0, hideDelay: 0, preferredPlacement: .Above, maximumWidth: 300, contentPadding: 10, onOpened: vm.opened)\n PushButton(id: \"anchor\", text: \"Anchor\")\n } }");
    if(!code)throw std::runtime_error(code.error().message);
    for(auto token:{"#include <LVUI/NativeTooltip.h>","lvui::native::NativeTooltip::create(","->set_target(","->content_root()","tooltip.refresh()"})
        if(code->find(token)==code->npos)throw std::runtime_error(std::string("Generated Tooltip lost ")+token);
    if(code->find("lvrs::")!=code->npos||code->find("<LVRS/")!=code->npos||code->find("<Qt")!=code->npos)
        throw std::runtime_error("Generated Tooltip refers to legacy code");
    auto inline_content=lvui::compile_native("Window { VStack {\n Tooltip(targetId: \"anchor\") { Label(text: \"Help\") }\n PushButton(id: \"anchor\")\n } }");
    if(!inline_content||inline_content->find("->content_root()")==inline_content->npos)throw std::runtime_error("Inline Tooltip content was not generated");
    for(auto source:{"Window { Tooltip(delay: -1) }","Window { Tooltip(preferredPlacement: .Bad) }",
                     "Window { Tooltip(scrollContent: true) }","Window { Tooltip(motionEnabled: true) }",
                     "Window { Tooltip(text: \"Help\") { Label(text: \"Other\") } }",
                     "import vm Model\nWindow { Tooltip(maximumWidth: vm.width) }"})
        if(lvui::compile_native(source))throw std::runtime_error("Unsupported Tooltip input was accepted");
}
