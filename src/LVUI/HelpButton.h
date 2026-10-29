#pragma once
#include "Button.h"
namespace lvui {
// A help action uses the shared LVUI button policy with its own design preset.
[[nodiscard]] inline ButtonConfiguration help_button_configuration(){
    ButtonConfiguration value;value.text="?";value.accessibility_name="Help";value.tone=ButtonTone::normal;
    value.width=21;value.height=21;value.horizontal_padding=4;value.corner_radius=100;
    value.font_size=12;value.line_height=12;value.font_weight=600;value.text_vertical_offset=.5;
    value.palette.normal={.0951643139,.0965511352,.0979379490,1};
    value.palette.text={1,1,1,.6};return value;
}
} // namespace lvui
