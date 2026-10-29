#pragma once
#include "ColorSource.h"
#include "Text.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
enum class ColorPickerButtonSize { small=22,medium=28,large=36 };
struct ColorPickerButtonPalette final {
    StyleColor normal{.12302529811859131,.12528030574321747,.12753531336784363,1};
    StyleColor hover{.19355684518814087,.1978072077035904,.19993239641189575,1};
    StyleColor pressed{.08394163101911545,.08394163101911545,.08631309866905212,1};
    StyleColor border{1,1,1,.2},focus{.08120492845773697,.5099760294,1,1};
    friend bool operator==(const ColorPickerButtonPalette&,const ColorPickerButtonPalette&)=default;
};
struct ColorPickerButtonConfiguration final {
    StyleColor current_color{.5275065302848816,.291259765625,1,1};
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

class ColorPickerButton final {
    ColorPickerButtonConfiguration value_;
    [[nodiscard]] static bool scalar(double value,double low,double high)noexcept{return std::isfinite(value)&&value>=low&&value<=high;}
public:
    [[nodiscard]] static bool valid_color(StyleColor value)noexcept{
        return valid_color_value(value);
    }
    [[nodiscard]] static bool validate(const ColorPickerButtonConfiguration& value)noexcept{
        const auto& p=value.palette;
        return valid_color(value.current_color)&&scalar(value.size,8,256)&&scalar(value.padding,0,128)&&scalar(value.corner_radius,0,256)&&
            scalar(value.border_opacity,0,1)&&valid_accessibility_text(value.accessibility_name)&&valid_color(p.normal)&&valid_color(p.hover)&&
            valid_color(p.pressed)&&valid_color(p.border)&&valid_color(p.focus);
    }
    [[nodiscard]] const ColorPickerButtonConfiguration& configuration()const noexcept{return value_;}
    bool set_configuration(ColorPickerButtonConfiguration value){if(!validate(value))return false;value_=std::move(value);return true;}
    [[nodiscard]] std::optional<ColorPickerButtonGeometry> geometry(double width,double height)const noexcept{
        if(!scalar(width,0,8192)||!scalar(height,0,8192))return std::nullopt;
        const auto short_side=std::min(width,height),well=std::max(0.,short_side-2*value_.padding),dot=well*(18./28.);
        const auto centered=[&](double size){return ColorPickerButtonRect{(width-size)/2,(height-size)/2,size,size};};
        return ColorPickerButtonGeometry{{0,0,width,height},centered(well),centered(dot),
            std::min(value_.circular?short_side/2:value_.corner_radius,short_side/2),dot/2};
    }
    [[nodiscard]] ColorPickerButtonVisual visual(ColorPickerButtonInteraction interaction)const noexcept{
        const bool enabled=value_.enabled&&interaction.enabled;const auto& palette=value_.palette;
        auto border=enabled&&interaction.focused?palette.focus:palette.border;border.alpha*=value_.border_opacity;
        return {enabled?(interaction.pressed?palette.pressed:interaction.hovered?palette.hover:palette.normal):palette.normal,
            border,value_.current_color,enabled&&interaction.focused?2.:.5,enabled?1.:.32};
    }
};
} // namespace lvui
