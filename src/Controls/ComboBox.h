#pragma once
#include <LVUI/Button.h>
#include <LVUI/Stepper.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace lvui {
struct ComboBoxPalette final {
    StyleColor surface{figma_defaults::panel_surface_input};
    StyleColor hover{figma_defaults::panel_surface_input_hover};
    StyleColor pressed{figma_defaults::panel_surface_raised};
    StyleColor disabled{figma_defaults::panel_sub_surface};
    StyleColor text{1,1,1,1};
    StyleColor disabled_text{1,1,1,.3};
    StyleColor focus{figma_defaults::accent_blue_default};
    friend bool operator==(const ComboBoxPalette&,const ComboBoxPalette&)=default;
};
struct ComboBoxConfiguration final {
    std::string text="Label",accessibility_name;
    ButtonTone tone=ButtonTone::primary;
    StepperArrow arrow=StepperArrow::up_down;
    bool enabled=true;
    double width=97,height=20,left_padding=8,right_padding=1,indicator_size=18,corner_radius=5;
    double font_size=13,line_height=13;
    unsigned font_weight=500;
    ComboBoxPalette palette;
    friend bool operator==(const ComboBoxConfiguration&,const ComboBoxConfiguration&)=default;
};
struct ComboBoxGeometry final {
    ButtonRect surface,label,indicator;
    double radius=0;
};
struct ComboBoxVisual final {
    StyleColor background,text,focus;
    bool enabled=true,focused=false;
};
struct ComboBoxModelPolicy final {
    using Configuration=ComboBoxConfiguration;using Geometry=ComboBoxGeometry;using Visual=ComboBoxVisual;
    using Rect=ButtonRect;using Interaction=ButtonInteraction;using Tone=ButtonTone;using Arrow=StepperArrow;using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept {return valid_accessibility_text(value);}
};
} // namespace lvui
