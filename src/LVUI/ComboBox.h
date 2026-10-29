#pragma once
#include "Button.h"
#include "Stepper.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace lvui {
struct ComboBoxPalette final {
    StyleColor surface{.1555851549,.1570725590,.1585599482,1};
    StyleColor hover{.1753742844,.1819191724,.1841008067,1};
    StyleColor pressed{.1935568452,.1978072077,.1999323964,1};
    StyleColor disabled{.0951643139,.0965511352,.0979379490,1};
    StyleColor text{1,1,1,1};
    StyleColor disabled_text{1,1,1,.3};
    StyleColor focus{.0812049285,.5099760294,1,1};
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
// A click field: the application owns its menu and selected value.
class ComboBox final {
    ComboBoxConfiguration configuration_;
public:
    [[nodiscard]] static bool validate(const ComboBoxConfiguration& v)noexcept{
        const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return range(c.red,0,1)&&range(c.green,0,1)&&range(c.blue,0,1)&&range(c.alpha,0,1);};
        if(!valid_accessibility_text(v.text)||!valid_accessibility_text(v.accessibility_name)||
           !range(v.width,19,8192)||!range(v.height,1,8192)||!range(v.left_padding,0,8192)||
           !range(v.right_padding,0,8192)||!range(v.indicator_size,1,256)||!range(v.corner_radius,0,8192)||
           !range(v.font_size,1,256)||!range(v.line_height,1,8192)||
           (v.font_weight!=400&&v.font_weight!=500&&v.font_weight!=600&&v.font_weight!=700)||
           (v.tone!=ButtonTone::primary&&v.tone!=ButtonTone::borderless))return false;
        switch(v.arrow){case StepperArrow::up_down:case StepperArrow::up:case StepperArrow::down:break;default:return false;}
        const auto& p=v.palette;return color(p.surface)&&color(p.hover)&&color(p.pressed)&&color(p.disabled)&&
            color(p.text)&&color(p.disabled_text)&&color(p.focus);
    }
    [[nodiscard]] bool set_configuration(ComboBoxConfiguration value){
        if(!validate(value))return false;configuration_=std::move(value);return true;
    }
    [[nodiscard]] const ComboBoxConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] ComboBoxVisual visual(ButtonInteraction interaction)const noexcept{
        const auto& p=configuration_.palette;const auto enabled=configuration_.enabled&&interaction.enabled;
        const auto background=!enabled?p.disabled:interaction.pressed?p.pressed:interaction.hovered?p.hover:
            configuration_.tone==ButtonTone::borderless?StyleColor{0,0,0,0}:p.surface;
        return {background,enabled?p.text:p.disabled_text,p.focus,enabled,enabled&&interaction.focused};
    }
    [[nodiscard]] std::optional<ComboBoxGeometry> geometry(double width,double height)const noexcept{
        if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192)return std::nullopt;
        const auto& v=configuration_;ComboBoxGeometry result;result.surface={0,0,width,height};
        result.radius=std::min({v.corner_radius,width/2,height/2});
        const auto right=std::min(v.right_padding,width),size=std::min({v.indicator_size,width-right,height});
        const auto indicator_x=width-right-size;
        result.indicator={indicator_x,(height-size)/2,size,size};
        const auto left=std::min(v.left_padding,indicator_x),line=std::min(v.line_height,height);
        result.label={left,(height-line)/2,std::max(0.,indicator_x-left),line};return result;
    }
    [[nodiscard]] std::string_view glyph()const noexcept{
        return configuration_.arrow==StepperArrow::up?"⌃":configuration_.arrow==StepperArrow::down?"⌄":"↕";
    }
};
} // namespace lvui
