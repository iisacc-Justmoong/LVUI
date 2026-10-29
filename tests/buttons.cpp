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
    auto styled=compile("import vm Model\nWindow(accent: vm.accent) { PushButton(width: vm.width, primaryColor: vm.primary, hoverColor: \"#336699\", pressedColor: \"#335577\", normalColor: \"#111111\", normalHoverColor: \"#222222\", normalPressedColor: \"#333333\", dangerColor: \"#ff0000\", disabledBackgroundColor: \"#444444\", textColor: \"#eeeeee\", disabledTextColor: \"#777777\", borderColor: vm.border, borderWidth: 1, fontSize: 14, lineHeight: 18, iconSpacing: 6, textOffsetY: -1) }");
    for(auto field:{".configuration.width =", ".configuration.palette.primary =", ".configuration.palette.primary_hover =",
                    ".configuration.palette.primary_pressed =", ".configuration.palette.normal =", ".configuration.palette.normal_hover =",
                    ".configuration.palette.normal_pressed =", ".configuration.palette.danger =", ".configuration.palette.disabled_background =",
                    ".configuration.palette.text =", ".configuration.palette.disabled_text =", ".configuration.border_color =",
                    ".configuration.border_width =", ".configuration.font_size =", ".configuration.line_height =",
                    ".configuration.icon_spacing =", ".configuration.text_vertical_offset ="})
        require(styled.find(field)!=styled.npos,"A usable button appearance input was not generated");
    for(auto source:{"Window { PushButton(width: 0) }", "Window { PushButton(borderWidth: 1) }",
                     "Window { PushButton(primaryColor: \"invalid\") }", "Window { PushButton(fontSize: 0) }"})
        require(!lvui::compile_native(source),"An invalid button appearance was accepted");
    for(auto source:{"import vm Model\nWindow { IconButton(icon: vm.icon, iconName: \"home\") }",
                     "import vm Model\nWindow { IconButton(iconSource: vm.source) }",
                     "Window { IconButton(iconName: \"home\", iconRevision: 1.5) }"})
        require(!lvui::compile_native(source),"Ambiguous or invalid icon inputs were accepted");
}
