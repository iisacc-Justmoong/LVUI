#pragma once
#include <LVUI/Button.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
namespace lvui {
struct DropdownConfiguration final {
    std::string text,accessibility_name;
    bool enabled=true,icon_mode=false;
    ButtonTone tone=ButtonTone::primary;
    std::optional<double> width;
    double height=22,left_padding=8,right_padding=2,spacing=0,corner_radius=8;
    double icon_size=18,indicator_size=18,font_size=13,line_height=13;
    unsigned font_weight=500;
    ButtonPalette palette;
    friend bool operator==(const DropdownConfiguration&,const DropdownConfiguration&)=default;
};
struct DropdownGeometry final {ButtonRect surface,label,icon,indicator;double radius=0;};
} // namespace lvui
