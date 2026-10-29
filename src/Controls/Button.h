#pragma once
#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
enum class ButtonTone { primary, normal, borderless, destructive, disabled };
struct ButtonPalette final {
    StyleColor primary{.0812049285,.5099760294,1,1};
    StyleColor primary_hover{.0725044012,.4553357363,.8928571343,1};
    StyleColor primary_pressed{.0676707774,.4249800146,.8333333135,1};
    StyleColor normal{.1935568452,.1978072077,.1999323964,1};
    StyleColor normal_hover{.1230252981,.1252803057,.1275353134,1};
    StyleColor normal_pressed{.1315810233,.2001844049,.2785882354,1};
    StyleColor danger{1,.2760854959,.2760854959,1};
    StyleColor disabled_background{.0951643139,.0965511352,.0979379490,1};
    StyleColor text{1,1,1,.8};
    StyleColor disabled_text{1,1,1,.3};
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
    ButtonPalette palette;
    std::optional<StyleColor> border_color;
    double border_width = 0;
    friend bool operator==(const ButtonConfiguration&,const ButtonConfiguration&) = default;
};
struct ButtonInteraction final {
    bool enabled = true, hovered = false, pressed = false, focused = false;
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

// The host supplies interaction and text measurements; the button owns the
// state policy, visual choice and arrangement independently of that host.
class Button final {
public:
    [[nodiscard]] const ButtonConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] static bool validate(const ButtonConfiguration& value) noexcept {
        const auto bound=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return bound(c.red,0,1)&&bound(c.green,0,1)&&bound(c.blue,0,1)&&bound(c.alpha,0,1);};
        if(!valid_accessibility_text(value.text)||!valid_accessibility_text(value.accessibility_name)||
           !bound(value.height,1,8192)||(value.width&&!bound(*value.width,1,8192))||!bound(value.horizontal_padding,0,8192)||
           !bound(value.corner_radius,0,8192)||!bound(value.icon_size,1,256)||
           !bound(value.icon_spacing,0,8192)||!bound(value.font_size,1,256)||!bound(value.line_height,0,8192)||
           !bound(value.text_vertical_offset,-8192,8192)||!bound(value.border_width,0,8192)||
           (value.border_color&&!color(*value.border_color))||
           (value.font_weight!=400&&value.font_weight!=500&&value.font_weight!=600&&value.font_weight!=700)||
           (value.icon_mode&&(!value.has_icon||value.horizontal_padding+value.icon_size+
             (value.text.empty()?0:value.icon_spacing)>8192))) return false;
        switch(value.tone){case ButtonTone::primary:case ButtonTone::normal:case ButtonTone::borderless:
            case ButtonTone::destructive:case ButtonTone::disabled:break;default:return false;}
        const auto& p=value.palette;
        return color(p.primary)&&color(p.primary_hover)&&color(p.primary_pressed)&&color(p.normal)&&
               color(p.normal_hover)&&color(p.normal_pressed)&&color(p.danger)&&color(p.disabled_background)&&
               color(p.text)&&color(p.disabled_text);
    }
    bool set_configuration(ButtonConfiguration value) {
        if(!validate(value))return false;
        configuration_=std::move(value);return true;
    }
    [[nodiscard]] double icon_gap() const noexcept {
        return configuration_.icon_mode&&!configuration_.text.empty()&&configuration_.tone!=ButtonTone::primary
            ?configuration_.icon_spacing:0;
    }
    [[nodiscard]] ButtonVisual visual(ButtonInteraction interaction) const noexcept {
        const auto& p=configuration_.palette;
        const bool enabled=configuration_.enabled&&interaction.enabled&&configuration_.tone!=ButtonTone::disabled;
        ButtonVisual result{enabled,enabled&&interaction.focused,p.primary,p.text,p.primary};
        if(!enabled){result.background=p.disabled_background;result.text=p.disabled_text;return result;}
        const auto darken=[](StyleColor c,double factor){return StyleColor{c.red/factor,c.green/factor,c.blue/factor,c.alpha};};
        switch(configuration_.tone){
        case ButtonTone::primary:
            result.background=interaction.pressed?p.primary_pressed:interaction.hovered?p.primary_hover:p.primary;break;
        case ButtonTone::normal:
            result.background=interaction.pressed?p.normal_pressed:interaction.hovered?p.normal_hover:p.normal;break;
        case ButtonTone::borderless:
            result.text=p.primary;result.background=interaction.pressed?p.normal_pressed:interaction.hovered?p.normal_hover:StyleColor{0,0,0,0};break;
        case ButtonTone::destructive:
            result.background=interaction.pressed?darken(p.danger,1.2):interaction.hovered?darken(p.danger,1.12):p.danger;break;
        case ButtonTone::disabled:break;
        }
        return result;
    }
    [[nodiscard]] std::optional<ButtonGeometry> geometry(double width,double height,double text_width,double text_height) const noexcept {
        const auto extent=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        const auto metric=[](double n){return std::isfinite(n)&&n>=0;};
        if(!extent(width)||!extent(height)||!metric(text_width)||!metric(text_height))return std::nullopt;
        ButtonGeometry result;
        result.surface={0,0,width,height};
        result.radius=std::min(configuration_.corner_radius,std::min(width,height)/2);
        if(width==0||height==0)return result;
        const auto padding=std::min(configuration_.horizontal_padding,width/2);
        const auto available=std::max(0.0,width-2*padding);
        const bool has_text=!configuration_.text.empty()&&text_width>0&&text_height>0;
        const auto icon=configuration_.icon_mode?std::min({configuration_.icon_size,available,height}):0;
        const auto gap=icon>0&&has_text?std::min(icon_gap(),std::max(0.0,available-icon)):0;
        const auto fitted_text=has_text?std::min(text_width,std::max(0.0,available-icon-gap)):0;
        const auto total=icon+gap+fitted_text;
        const auto left=(width-total)/2;
        result.icon={left,(height-icon)/2,icon,icon};
        const auto fitted_height=std::min(height,text_height);
        result.text={left+icon+gap,(height-fitted_height)/2+configuration_.text_vertical_offset,fitted_text,fitted_height};
        result.draw_icon=icon>0;result.draw_text=fitted_text>0&&fitted_height>0;
        return result;
    }
private:
    ButtonConfiguration configuration_;
};
} // namespace lvui
