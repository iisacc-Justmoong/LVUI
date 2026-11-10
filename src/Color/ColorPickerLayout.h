#pragma once

#include <LVUI/ColorChannels.h>
#include <LVUI/ColorPicker.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace lvui {

struct ColorPickerRect final {
    double x = 0, y = 0, width = 0, height = 0;
    friend bool operator==(const ColorPickerRect&, const ColorPickerRect&) = default;
};

struct ColorPickerLayout final {
    double width = 0, height = 0;
    ColorSurfaceKind surface_kind = ColorSurfaceKind::wheel;
    std::optional<ColorSurfaceKind> rail_kind;
    ColorPickerRect surface, rail, divider, comparison, hex_label, hex_field;
    ColorPickerRect alpha_label, alpha_field, alpha_suffix, recent_label;
    ColorPickerRect eyedropper, color_space, cancel, apply;
    std::vector<std::pair<ColorChannel, ColorPickerRect>> channels;
    std::vector<ColorPickerRect> recent;
};

// Design dimensions are supplied to the independent iiGUI geometry implementation.
struct ColorPickerLayoutMetrics final {
 double section_gap=12,channel_height=22,channel_gap=4,plane_reserve=36,wheel_max=232,spectrum_height=184,rail_width=24;
 double divider_height=1,values_height=28,comparison_width=40,label_leading=48,hex_label_width=25,hex_field_leading=81,field_top=3;
 double hex_field_min=40,hex_field_reserve=187,alpha_label_reserve=98,alpha_label_width=10,alpha_field_reserve=80,alpha_field_width=56;
 double suffix_reserve=16,suffix_width=16,recent_height=20,recent_label_width=40,recent_gap=8,recent_leading=48;
 double action_height=22,action_icon_width=22,action_button_width=68,action_button_gap=8,color_space_leading=30,color_space_reserve=38;
};
struct ColorPickerLayoutPolicy final {
 using Configuration=ColorPickerConfiguration;using Layout=ColorPickerLayout;using Rect=ColorPickerRect;
 using Mode=ColorPickerMode;using Channel=ColorChannel;using Kind=ColorSurfaceKind;
 [[nodiscard]] static ColorPickerLayoutMetrics metrics()noexcept{return {};}
 [[nodiscard]] static bool valid(const Configuration& value)noexcept{return ColorPicker::validate(value);}
};
template<template<class>class Implementation>using ColorPickerLayoutBinding=Implementation<ColorPickerLayoutPolicy>;

} // namespace lvui
