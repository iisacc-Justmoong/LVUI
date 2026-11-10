#pragma once
#include <LVUI/ColorPickerButton.h>
#include <LVUI/BuiltinHueRing.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <type_traits>
namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor color_button_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
[[nodiscard]] inline double color_button_size(ColorPickerButtonSize value)noexcept{return double(static_cast<int>(value));}
template<class T>requires std::is_arithmetic_v<T>
[[nodiscard]] inline double color_button_size(T value)noexcept{return double(value);}
struct ColorPickerButtonInput final {
    ColorPickerButtonConfiguration configuration;std::shared_ptr<const ColorSource> source;
    std::int64_t source_revision=0;bool require_source=false;
    friend bool operator==(const ColorPickerButtonInput&,const ColorPickerButtonInput&)=default;
};
struct NativeColorPickerButtonCallbacks final {std::function<void()> clicked,pressed,released,canceled;};
template<class NativeModel>struct ColorPickerButtonHostPolicy final {
    using Model=NativeModel;using Input=ColorPickerButtonInput;using Callbacks=NativeColorPickerButtonCallbacks;
    using Color=StyleColor;using Rect=ColorPickerButtonRect;
    [[nodiscard]] static std::span<const std::uint8_t> hue_ring()noexcept{return lvui::detail::color_picker_hue_ring;}
};
template<template<class>class NativeModel,template<class>class NativeHost>
using ColorPickerButtonBinding=NativeHost<ColorPickerButtonHostPolicy<NativeModel<ColorPickerButtonModelPolicy>>>;
} // namespace lvui::native
