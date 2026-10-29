#pragma once
#include <LVUI/StyleTokens.h>
#include <LVUI/TextInput.h>
#include <cmath>
#include <string>

namespace lvui {
struct InputFieldPalette final {
    StyleColor background{.155585,.157073,.15856,1};
    StyleColor hover{.175374,.181919,.184101,1};
    StyleColor text{1,1,1,.9};
    StyleColor placeholder{1,1,1,.5};
    StyleColor selection{.0812049285,.5099760294,1,.38};
    StyleColor caret{1,1,1,1};
    StyleColor focus{.0812049285,.5099760294,1,1};
    friend bool operator==(const InputFieldPalette&,const InputFieldPalette&)=default;
};
struct InputFieldConfiguration final {
    std::string text,placeholder,accessibility_name="Text field",font_family;
    double width=206,height=30,horizontal_padding=8,vertical_padding=4,corner_radius=8,font_size=13,line_height=0;
    bool enabled=true,read_only=false,multiline=false,password=false;
    InputFieldPalette palette;
    friend bool operator==(const InputFieldConfiguration&,const InputFieldConfiguration&)=default;
};
[[nodiscard]] inline bool valid_input_field_configuration(const InputFieldConfiguration& value) noexcept {
    const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
    const auto color=[&](StyleColor c){return range(c.red,0,1)&&range(c.green,0,1)&&range(c.blue,0,1)&&range(c.alpha,0,1);};
    return valid_accessibility_text(value.text)&&valid_accessibility_text(value.placeholder)&&
        valid_accessibility_text(value.accessibility_name)&&valid_accessibility_text(value.font_family)&&value.font_family.size()<=256&&
        range(value.width,1,8192)&&range(value.height,1,8192)&&range(value.horizontal_padding,0,8192)&&
        range(value.vertical_padding,0,8192)&&range(value.corner_radius,0,8192)&&range(value.font_size,1,256)&&
        range(value.line_height,0,1024)&&color(value.palette.background)&&color(value.palette.hover)&&color(value.palette.text)&&
        color(value.palette.placeholder)&&color(value.palette.selection)&&color(value.palette.caret)&&color(value.palette.focus);
}
} // namespace lvui
