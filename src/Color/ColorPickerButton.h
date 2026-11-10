#pragma once
#include <LVUI/ColorSource.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
enum class ColorPickerButtonSize { small=22,medium=28,large=36 };
struct ColorPickerButtonPalette final {
    StyleColor normal{figma_defaults::panel_surface_alt};
    StyleColor hover{figma_defaults::panel_surface_raised};
    StyleColor pressed{figma_defaults::panel_window_alt};
    StyleColor border{1,1,1,.2},focus{figma_defaults::accent_blue_default};
    friend bool operator==(const ColorPickerButtonPalette&,const ColorPickerButtonPalette&)=default;
};
struct ColorPickerButtonConfiguration final {
    StyleColor current_color{figma_defaults::accent_purple_default};
    double size=28,padding=4,corner_radius=4,border_opacity=1;
    bool enabled=true,show_hue_ring=true,circular=false;
    std::string accessibility_name="Choose color";
    ColorPickerButtonPalette palette;
    friend bool operator==(const ColorPickerButtonConfiguration&,const ColorPickerButtonConfiguration&)=default;
};
struct ColorPickerButtonRect final {double x=0,y=0,width=0,height=0;};
struct ColorPickerButtonGeometry final {
    ColorPickerButtonRect surface,ring,swatch;
    double radius=0,swatch_radius=0;
};
struct ColorPickerButtonInteraction final {bool enabled=true,hovered=false,pressed=false,focused=false;};
struct ColorPickerButtonVisual final {StyleColor background,border,swatch;double border_width=.5,opacity=1;};

struct ColorPickerButtonModelPolicy final {
    using Configuration=ColorPickerButtonConfiguration;using Rect=ColorPickerButtonRect;using Geometry=ColorPickerButtonGeometry;
    using Interaction=ColorPickerButtonInteraction;using Visual=ColorPickerButtonVisual;using Color=StyleColor;
    [[nodiscard]] static bool valid_color(Color value)noexcept{return valid_color_value(value);}
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
} // namespace lvui
