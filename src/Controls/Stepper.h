#pragma once
#include <LVUI/Button.h>
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
} // namespace lvui
