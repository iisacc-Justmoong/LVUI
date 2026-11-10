#pragma once
#include <LVUI/CheckBox.h>
#include <LVUI/NativeAuthoredIcons.h>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace lvui::native {
struct CheckBoxModelPolicy final {
    using Configuration=CheckBoxConfiguration;using Interaction=CheckBoxInteraction;using Visual=CheckBoxVisual;
    using Geometry=CheckBoxGeometry;using Color=StyleColor;using Shape=CheckBoxShape;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
    [[nodiscard]] static bool authored_mark(const Configuration& c)noexcept {
        return c.use_figma_checked_assets.value_or(c.palette.checked==figma_defaults::accent_blue_default);
    }
    [[nodiscard]] static Color authored_mark_color(const Configuration&,bool enabled)noexcept {
        return {1,1,1,enabled?.8:.3};
    }
};
template<class Color>
inline StyleColor checkbox_color(const Color& value)noexcept{
    return {value.red,value.green,value.blue,value.alpha};
}
inline Motion checkbox_motion_duration(Motion value,double milliseconds)noexcept{
    if(!std::isfinite(milliseconds)||milliseconds<0||milliseconds>86400000||std::floor(milliseconds)!=milliseconds)
        value.duration=std::chrono::milliseconds{-1};
    else value.duration=std::chrono::milliseconds{static_cast<std::int64_t>(milliseconds)};
    return value;
}
inline CheckBoxShape checkbox_shape(CheckBoxShape value)noexcept{return value;}
template<class T> requires (std::is_arithmetic_v<T> && !std::is_same_v<std::remove_cv_t<T>,bool>)
inline CheckBoxShape checkbox_shape(T value)noexcept{
    if(value==0)return CheckBoxShape::round_rect;
    if(value==1)return CheckBoxShape::cylinder;
    return static_cast<CheckBoxShape>(-1);
}

template<class NativeModel,class Decoder>
struct CheckBoxHostPolicy final {
    using Model=NativeModel;using Configuration=CheckBoxConfiguration;using Interaction=CheckBoxInteraction;
    using Rect=CheckBoxRect;using Color=StyleColor;
    [[nodiscard]] static bool authored_mark(const CheckBoxVisual& value)noexcept{return value.figma_mark;}
    [[nodiscard]] static auto authored_image(bool enabled){
        return authored_icon_image<Decoder>(enabled?"checkboxCheckedEnabled":"checkboxCheckedDisabled",68);
    }
};
template<template<class> class NativeModel,template<class> class NativeHost,class Decoder>
using CheckBoxBinding=NativeHost<CheckBoxHostPolicy<NativeModel<CheckBoxModelPolicy>,Decoder>>;

} // namespace lvui::native
