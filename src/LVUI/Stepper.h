#pragma once
#include "Button.h"
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace lvui {
enum class StepperArrow { up_down, up, down };
struct StepperConfiguration final {
    StepperArrow arrow=StepperArrow::up_down;
    ButtonTone tone=ButtonTone::primary;
    bool enabled=true;
    double width=18,height=18,corner_radius=4;
    std::string accessibility_name="Stepper";
    ButtonPalette palette=[] { ButtonPalette value;value.text={1,1,1,1};value.disabled_text={1,1,1,.38};return value; }();
    std::optional<StyleColor> hover_color,pressed_color,disabled_color,text_color,disabled_text_color;
    friend bool operator==(const StepperConfiguration&,const StepperConfiguration&)=default;
};
// The Stepper emits a direction; it does not own the application's numeric value.
class Stepper final {
    StepperConfiguration configuration_;
    [[nodiscard]] static ButtonConfiguration button(const StepperConfiguration& value){
        ButtonConfiguration result;result.enabled=value.enabled;result.tone=value.tone;
        result.height=value.height;result.corner_radius=value.corner_radius;result.palette=value.palette;
        return result;
    }
public:
    [[nodiscard]] static bool validate(const StepperConfiguration& value)noexcept{
        if(!std::isfinite(value.width)||value.width<1||value.width>8192||
           !std::isfinite(value.height)||value.height<1||value.height>8192||
           !valid_accessibility_text(value.accessibility_name))return false;
        switch(value.arrow){case StepperArrow::up_down:case StepperArrow::up:case StepperArrow::down:break;default:return false;}
        const auto color=[](const std::optional<StyleColor>& c){
            if(!c)return true;const auto channel=[](double n){return std::isfinite(n)&&n>=0&&n<=1;};
            return channel(c->red)&&channel(c->green)&&channel(c->blue)&&channel(c->alpha);
        };
        if(!color(value.hover_color)||!color(value.pressed_color)||!color(value.disabled_color)||
           !color(value.text_color)||!color(value.disabled_text_color))return false;
        return Button::validate(button(value));
    }
    [[nodiscard]] bool set_configuration(StepperConfiguration value){
        if(!validate(value))return false;configuration_=std::move(value);return true;
    }
    [[nodiscard]] const StepperConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] bool accepts(int direction)const noexcept{
        return configuration_.enabled&&configuration_.tone!=ButtonTone::disabled&&
            (direction==1||direction==-1)&&
            (configuration_.arrow==StepperArrow::up_down||
             (configuration_.arrow==StepperArrow::up&&direction==1)||
             (configuration_.arrow==StepperArrow::down&&direction==-1));
    }
    [[nodiscard]] std::optional<int> direction_at(double y,double height)const noexcept{
        if(!std::isfinite(y)||!std::isfinite(height)||height<=0||y<0||y>height)return std::nullopt;
        const auto direction=configuration_.arrow==StepperArrow::up?1:configuration_.arrow==StepperArrow::down?-1:y<height/2?1:-1;
        return accepts(direction)?std::optional<int>{direction}:std::nullopt;
    }
    [[nodiscard]] ButtonVisual visual(ButtonInteraction interaction)const{
        Button control;(void)control.set_configuration(button(configuration_));auto result=control.visual(interaction);
        if(result.enabled&&configuration_.tone==ButtonTone::borderless)result.text=configuration_.palette.text;
        if(!result.enabled){if(configuration_.disabled_color)result.background=*configuration_.disabled_color;
            if(configuration_.disabled_text_color)result.text=*configuration_.disabled_text_color;}
        else{if(interaction.pressed&&configuration_.pressed_color)result.background=*configuration_.pressed_color;
            else if(interaction.hovered&&configuration_.hover_color)result.background=*configuration_.hover_color;
            if(configuration_.text_color)result.text=*configuration_.text_color;}
        return result;
    }
    [[nodiscard]] std::string_view glyph()const noexcept{
        return configuration_.arrow==StepperArrow::up?"⌃":configuration_.arrow==StepperArrow::down?"⌄":"↕";
    }
};
} // namespace lvui
