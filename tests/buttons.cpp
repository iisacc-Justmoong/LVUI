#include <LVUI/Compiler.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string compile(std::string_view source){auto result=lvui::compile_native(source);if(!result)throw std::runtime_error(result.error().message);return *result;}
}
int main(){
    auto code=compile("import vm Model\nWindow { VStack { PushButton(text: vm.text, enabled: vm.enabled, tone: vm.tone, onClick: vm.save)\nIconButton(icon: vm.icon, onClick: vm.save) } }");
    require(code.find("#include <LVUI/NativeButton.h>")!=code.npos && code.find("lvui::native::ButtonBinding<")!=code.npos &&
            code.find("lvui::native::ButtonInputValue<ii::gui::RasterImage>{")!=code.npos && code.find("component.set_options(value)")!=code.npos &&
            code.find("widgets.button(")==code.npos && code.find("lvrs::")==code.npos,"Button is not LVUI-owned");
    auto empty=lvui::compile_native("Window { IconButton() }");
    require(empty && empty->find("projectStructure") != empty->npos,
            "IconButton did not use the Figma default icon");
    require(!lvui::compile_native("Window { IconButton(iconName: \"\") }"),
            "An explicitly empty icon name was not diagnosed");
    auto named=compile("import vm Model\nWindow { IconButton(iconName: vm.iconName, iconSource: vm.source, iconRevision: vm.revision) }");
    require(named.find("value.icon_name = ")!=named.npos&&named.find("value.icon_source = ")!=named.npos&&
            named.find("value.icon_revision = ")!=named.npos,"Named icon inputs were not generated");
    require(named.find("lvui::icon_button_defaults::icon_size")!=named.npos,
            "IconButton did not use the shared Figma icon size");
    auto explicit_icon=compile("Window { IconButton(iconName: \"home\", iconSize: 12) }");
    require(explicit_icon.find(".icon_size = 12")!=explicit_icon.npos,
            "Explicit IconButton size did not take precedence over its default");
    auto labeled_icon=compile("Window { PushButton(text: \"Save\", iconMode: true, iconName: \"home\") }");
    require(labeled_icon.find(".icon_size = 16")!=labeled_icon.npos,
            "IconButton geometry changed the separate PushButton icon default");
    auto styled=compile("import vm Model\nWindow(accent: vm.accent) { PushButton(width: vm.width, primaryColor: vm.primary, hoverColor: \"#336699\", pressedColor: \"#335577\", normalColor: \"#111111\", normalHoverColor: \"#222222\", normalPressedColor: \"#333333\", dangerColor: \"#ff0000\", dangerHoverColor: vm.danger_hover, dangerPressedColor: \"#dd0000\", disabledBackgroundColor: \"#444444\", textColor: \"#eeeeee\", disabledTextColor: \"#777777\", borderColor: vm.border, borderWidth: 1, fontSize: 14, lineHeight: 18, iconSpacing: 6, textOffsetY: -1) }");
    for(auto field:{".configuration.width =", ".configuration.palette.primary =", ".configuration.palette.primary_hover =",
                    ".configuration.palette.primary_pressed =", ".configuration.palette.normal =", ".configuration.palette.normal_hover =",
                    ".configuration.palette.normal_pressed =", ".configuration.palette.danger =", ".configuration.palette.danger_hover =",
                    ".configuration.palette.danger_pressed =", ".configuration.palette.disabled_background =",
                    ".configuration.palette.text =", ".configuration.palette.disabled_text =", ".configuration.border_color =",
                    ".configuration.border_width =", ".configuration.font_size =", ".configuration.line_height =",
                    ".configuration.icon_spacing =", ".configuration.text_vertical_offset ="})
        require(styled.find(field)!=styled.npos,"A usable button appearance input was not generated");
    lvui::NativeCompileOptions red_theme;
    red_theme.style_tokens.overrides["Color.red.hover"]=lvui::StyleColor{.125,.25,.5,.75};
    red_theme.style_tokens.overrides["Color.red.pressed"]=lvui::StyleColor{.75,.5,.25,.125};
    for(const auto& [source,prefix]:{
            std::pair{"Window { AbstractButton(tone: .destructive) }","value.configuration.palette."},
            std::pair{"Window { PushButton(tone: .destructive) }","value.configuration.palette."},
            std::pair{"Window { LabelButton(tone: .destructive) }","value.configuration.palette."},
            std::pair{"Window { IconButton(iconName: \"home\", tone: .destructive) }","value.configuration.palette."},
            std::pair{"Window { HelpButton(tone: .destructive) }","value.configuration.palette."},
            std::pair{"Window { DropdownButton(tone: .destructive) }","value.configuration.palette."},
            std::pair{"Window { AlertButton(tone: .destructive) }","value.palette."},
            std::pair{"Window { Stepper(tone: .destructive) }","value.palette."}}){
        auto themed=lvui::compile_native(source,red_theme);
        require(themed&&themed->find(std::string(prefix)+"danger_reference = lvui::StyleColor{")!=themed->npos&&
            themed->find(std::string(prefix)+"danger_hover_default = lvui::StyleColor{0.125, 0.25, 0.5, 0.75}")!=themed->npos&&
            themed->find(std::string(prefix)+"danger_pressed_default = lvui::StyleColor{0.75, 0.5, 0.25, 0.125}")!=themed->npos,
            "A destructive button ignored the selected red state tokens");
    }
    auto custom_danger=lvui::compile_native("Window { PushButton(dangerColor: \"#ff0000\") }",red_theme);
    require(custom_danger&&custom_danger->find("value.configuration.palette.danger_hover =")==custom_danger->npos&&
            custom_danger->find("value.configuration.palette.danger_pressed =")==custom_danger->npos,
            "A custom danger color no longer derives its unspecified state colors");
    auto custom_state=lvui::compile_native("Window { PushButton(dangerHoverColor: \"#00ff00\") }",red_theme);
    require(bool(custom_state),"An explicit destructive hover color was rejected");
    const auto default_hover=custom_state->find("value.configuration.palette.danger_hover_default = lvui::StyleColor{0.125, 0.25, 0.5, 0.75}");
    const auto explicit_hover=custom_state->find("value.configuration.palette.danger_hover = lvui::native::button_color(ii::gui::WidgetColor{0, 1, 0, 1})");
    require(default_hover!=custom_state->npos&&explicit_hover!=custom_state->npos&&default_hover<explicit_hover,
            "An explicit destructive hover color did not override its common token");
    auto checked=compile("import vm Model\nWindow(accent: vm.accent) { LabelButton(text: \"Choice\", checkable: true, checked: vm.selected) }");
    for(auto field:{".configuration.checkable = true", ".configuration.checked =", "palette.selected = lvui::native::button_color(accent[4])"})
        require(checked.find(field)!=checked.npos,"Checkable button did not inherit the app selected color");
    require(checked.find("palette.normal_pressed = lvui::native::button_color(accent[4])")!=checked.npos,
            "Button press did not inherit the app selected color");
    auto segment=compile("import vm Model\nWindow { LabelSegmentedControl { LabelButton(text: \"Choice\", checked: vm.selected, selectionStyle: .segmented) } }");
    require(segment.find("value.configuration.selection_style = lvui::ButtonSelectionStyle::segmented")!=segment.npos,
            "Segmented selection style did not reach the native button");
    auto reactive_segment=compile("import vm Model\nWindow { LabelButton(selectionStyle: vm.style) }");
    require(reactive_segment.find("value.configuration.selection_style = lvui::native::button_selection_style(")!=reactive_segment.npos,
            "Reactive button selection style was not generated");
    require(!lvui::compile_native("Window { LabelButton(selectionStyle: .unknown) }"),
            "Unknown button selection style was accepted");
    for(auto source:{"import vm Model\nWindow { AbstractButton(motionEnabled: vm.motion) }",
                     "import vm Model\nWindow { PushButton(motionEnabled: vm.motion) }",
                     "import vm Model\nWindow { LabelButton(motionEnabled: vm.motion) }",
                     "import vm Model\nWindow { IconButton(iconName: \"home\", motionEnabled: vm.motion) }"}){
        auto motion=compile(source);
        require(motion.find("value.configuration.motion_enabled =")!=motion.npos,
                "Button motionEnabled did not reach the native configuration");
    }
    auto static_motion=compile("Window { PushButton(motionEnabled: false) }");
    require(static_motion.find("value.configuration.motion_enabled = false")!=static_motion.npos,
            "Static button motionEnabled was not generated");
    for(auto kind:{"AbstractButton","PushButton","LabelButton","IconButton","HelpButton","AlertButton"}){
        const std::string icon=std::string(kind)=="IconButton"?"iconName: \"home\", ":"";
        const auto head="import vm Model\nWindow { "+std::string(kind)+"("+icon;
        const auto code=compile(head+"enterKeyActivation: vm.enter) }");
        require(code.find("enter_key_activation =")!=code.npos,"Reactive Enter policy was not generated");
        require(!lvui::compile_native("Window { "+std::string(kind)+"("+icon+"enterKeyActivation: 1) }"),
                "Non-boolean Enter policy was accepted");
    }
    const auto abstract=compile("Window { AbstractButton() }");
    require(abstract.find("enter_key_activation = false")!=abstract.npos,"AbstractButton did not disable Enter by default");
    for(auto source:{"import vm Model\nWindow { AbstractButton(onPressed: vm.press, onReleased: vm.release, onCanceled: vm.cancel) }",
                     "import vm Model\nWindow { PushButton(text: vm.text, onPressed: vm.press, onReleased: vm.release, onCanceled: vm.cancel) }",
                     "import vm Model\nWindow { LabelButton(onPressed: vm.press, onReleased: vm.release, onCanceled: vm.cancel) }",
                     "import vm Model\nWindow { IconButton(iconName: \"home\", onPressed: vm.press, onReleased: vm.release, onCanceled: vm.cancel) }"}){
        auto events=compile(source);
        for(auto part:{"lvui::native::NativeButtonEvents{", "->set_events(", ".pressed =", ".released =", ".canceled ="})
            require(events.find(part)!=events.npos,"Button input event was not generated");
    }
    for(auto kind:{"AbstractButton","PushButton","LabelButton","IconButton","HelpButton","AlertButton",
                   "DropdownButton","IconMenuButton","LabelMenuButton","ComboBox","Stepper"}){
        const std::string prefix=std::string(kind)=="IconButton"?"iconName: \"home\", ":"";
        const auto head="import vm Model\nWindow { "+std::string(kind)+"("+prefix;
        auto methods=compile(head+"method: vm.first, methods: [vm.second, [vm.third]]) }");
        for(auto part:{"->set_method(","->set_methods("})
            require(methods.find(part)!=methods.npos,"Button method registry was not generated");
        require(methods.find("lvui::native::ButtonEventValue<ii::gui::Widget>")!=methods.npos,
                "Button method event type was not generated");
        auto dynamic=compile(head+"methods: vm.methods) }");
        require(dynamic.find("collect_button_methods<")!=dynamic.npos&&dynamic.find("component.set_methods(")!=dynamic.npos,
                "Reactive button method list was not generated");
    }
    for(auto source:{"Window { PushButton(width: 0) }", "Window { PushButton(borderWidth: 1) }",
                     "Window { PushButton(primaryColor: \"invalid\") }", "Window { PushButton(fontSize: 0) }",
                     "Window { PushButton(motionEnabled: 1) }"})
        require(!lvui::compile_native(source),"An invalid button appearance was accepted");
    for(auto source:{"import vm Model\nWindow { IconButton(icon: vm.icon, iconName: \"home\") }",
                     "import vm Model\nWindow { IconButton(iconSource: vm.source, iconName: \"\") }",
                     "Window { IconButton(iconName: \"home\", iconRevision: 1.5) }"})
        require(!lvui::compile_native(source),"Ambiguous or invalid icon inputs were accepted");
}
