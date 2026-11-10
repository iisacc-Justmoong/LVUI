#pragma once
#include <LVUI/RadioButton.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lvui::native {
struct RadioButtonModelPolicy final {
    using Configuration=RadioButtonConfiguration;using Interaction=RadioButtonInteraction;using Visual=RadioButtonVisual;
    using Geometry=RadioButtonGeometry;using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<class Color>
inline StyleColor radio_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
inline Motion radio_motion_duration(Motion value,double milliseconds)noexcept{
    if(!std::isfinite(milliseconds)||milliseconds<0||milliseconds>86400000||std::floor(milliseconds)!=milliseconds)
        value.duration=std::chrono::milliseconds{-1};
    else value.duration=std::chrono::milliseconds{static_cast<std::int64_t>(milliseconds)};
    return value;
}

template<class NativeModel>
struct RadioButtonHostPolicy final {
    using Model=NativeModel;using Configuration=RadioButtonConfiguration;using Rect=RadioButtonRect;using Color=StyleColor;
};
template<template<class> class NativeModel,template<class> class NativeHost>
using RadioButtonBinding=NativeHost<RadioButtonHostPolicy<NativeModel<RadioButtonModelPolicy>>>;

} // namespace lvui::native
