#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string compile(std::string_view source){auto result=lvui::compile_native(source);if(!result)throw std::runtime_error(result.error().message);return *result;}
}
int main(){
    auto code=compile("import vm Model\nWindow { VStack { PushButton(text: vm.text, enabled: vm.enabled, tone: vm.tone, onClick: vm.save)\nIconButton(icon: vm.icon, onClick: vm.save) } }");
    require(code.find("#include <LVUI/NativeButton.h>")!=code.npos && code.find("lvui::native::NativeButton::create(")!=code.npos &&
            code.find("lvui::native::ButtonInput{")!=code.npos && code.find("component.set_options(value)")!=code.npos &&
            code.find("widgets.button(")==code.npos && code.find("lvrs::")==code.npos,"Button is not LVUI-owned");
    auto empty=lvui::compile_native("Window { IconButton() }");
    require(!empty && empty.error().line>0,"Icon mode without an icon was not diagnosed");
    auto named=compile("import vm Model\nWindow { IconButton(iconName: vm.iconName, iconSource: vm.source, iconRevision: vm.revision) }");
    require(named.find("value.icon_name = ")!=named.npos&&named.find("value.icon_source = ")!=named.npos&&
            named.find("value.icon_revision = ")!=named.npos,"Named icon inputs were not generated");
    for(auto source:{"import vm Model\nWindow { IconButton(icon: vm.icon, iconName: \"home\") }",
                     "import vm Model\nWindow { IconButton(iconSource: vm.source) }",
                     "Window { IconButton(iconName: \"home\", iconRevision: 1.5) }"})
        require(!lvui::compile_native(source),"Ambiguous or invalid icon inputs were accepted");
}
