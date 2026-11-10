#pragma once
#include <LVUI/ToggleSwitch.h>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace lvui::native {
struct ToggleSwitchModelPolicy final {
    using Configuration=ToggleSwitchConfiguration;using Interaction=ToggleSwitchInteraction;using Visual=ToggleSwitchVisual;
    using Geometry=ToggleSwitchGeometry;using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
inline Motion toggle_motion_duration(Motion value,double milliseconds) noexcept {
    if(!std::isfinite(milliseconds)||milliseconds<0||milliseconds>86400000||std::floor(milliseconds)!=milliseconds)
        value.duration=std::chrono::milliseconds{-1};
    else value.duration=std::chrono::milliseconds{static_cast<std::int64_t>(milliseconds)};
    return value;
}
template<class Color>
inline StyleColor toggle_color(const Color& value) noexcept {
    return {value.red,value.green,value.blue,value.alpha};
}

template<class NativeModel>
struct ToggleSwitchHostPolicy final {
    using Model=NativeModel;using Configuration=ToggleSwitchConfiguration;using Rect=ToggleSwitchRect;using Color=StyleColor;
};
template<template<class> class NativeModel,template<class> class NativeHost>
using ToggleSwitchBinding=NativeHost<ToggleSwitchHostPolicy<NativeModel<ToggleSwitchModelPolicy>>>;
} // namespace lvui::native
