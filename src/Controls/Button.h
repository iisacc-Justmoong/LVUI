#pragma once
#include <LVUI/IconButtonDefaults.generated.h>
#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
enum class ButtonTone { primary, normal, borderless, destructive, disabled };
enum class ButtonSelectionStyle { accent, segmented };
struct ButtonPalette final {
    StyleColor primary{figma_defaults::accent_blue_default};
    StyleColor primary_hover{figma_defaults::accent_blue_hover};
    StyleColor primary_pressed{figma_defaults::accent_blue_pressed};
    StyleColor normal{figma_defaults::panel_surface_solid};
    StyleColor normal_hover{figma_defaults::panel_surface_alt};
    StyleColor normal_pressed{figma_defaults::accent_blue_selected};
    // An explicit token may override the selected color; otherwise it follows primary.
    std::optional<StyleColor> selected;
    StyleColor danger{figma_defaults::accent_red_default};
    StyleColor disabled_background{figma_defaults::panel_sub_surface};
    StyleColor text{figma_defaults::text_title_header_color};
    StyleColor disabled_text{figma_defaults::text_disabled_color};
    std::optional<StyleColor> danger_hover, danger_pressed;
    // A theme owns these defaults; changing danger alone still derives states.
    StyleColor danger_reference{figma_defaults::accent_red_default};
    StyleColor danger_hover_default{figma_defaults::accent_red_hover};
    StyleColor danger_pressed_default{figma_defaults::accent_red_pressed};
    // The application supplies the reference values for selected-state blending.
    StyleColor selected_surface{figma_defaults::panel_surface_interactive};
    StyleColor selected_reference{figma_defaults::accent_blue_default};
    StyleColor selected_default{figma_defaults::accent_blue_selected};
    friend bool operator==(const ButtonPalette&,const ButtonPalette&) = default;
};
struct ButtonConfiguration final {
    std::string text, accessibility_name;
    bool enabled = true;
    ButtonTone tone = ButtonTone::primary;
    double height = 22;
    std::optional<double> width;
    double horizontal_padding = 8, corner_radius = 8;
    double icon_size = 16, icon_spacing = 7, font_size = 13, line_height = 13;
    double text_vertical_offset = 0;
    unsigned font_weight = 500;
    bool icon_mode = false, has_icon = false;
    bool checkable = false, checked = false;
    ButtonPalette palette;
    std::optional<StyleColor> border_color;
    double border_width = 0;
    bool motion_enabled = true;
    bool enter_key_activation = true;
    ButtonSelectionStyle selection_style = ButtonSelectionStyle::accent;
    friend bool operator==(const ButtonConfiguration&,const ButtonConfiguration&) = default;
};
[[nodiscard]] inline ButtonConfiguration icon_button_configuration() {
    ButtonConfiguration result;
    result.icon_mode=true;
    result.height=icon_button_defaults::extent;
    result.icon_size=icon_button_defaults::icon_size;
    result.horizontal_padding=icon_button_defaults::horizontal_padding;
    result.corner_radius=icon_button_defaults::corner_radius;
    return result;
}
struct ButtonInteraction final {
    bool enabled = true, hovered = false, pressed = false, focused = false, checked = false;
};
struct ButtonVisual final {
    bool enabled = true, focused = false;
    StyleColor background, text, focus;
};
struct ButtonRect final { double x=0,y=0,width=0,height=0; };
struct ButtonGeometry final {
    ButtonRect surface, icon, text;
    double radius = 0;
    bool draw_icon = false, draw_text = false;
};

} // namespace lvui
