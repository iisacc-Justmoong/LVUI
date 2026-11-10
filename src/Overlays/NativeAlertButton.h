#pragma once
#include <LVUI/AlertButton.h>
#include <LVUI/NativeButton.h>
#include <functional>
#include <string_view>
#include <vector>

namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor alert_button_color(const Color& value) noexcept {
    return {value.red,value.green,value.blue,value.alpha};
}
template<class Widget>struct NativeAlertButtonCallbacksValue final {
    using Event=ButtonEventValue<Widget>;
    std::function<void(const Event&)> method;
    std::function<void()> clicked,pressed,released,canceled;
    std::vector<ButtonMethodValue<Event>> methods;
};
// Application-authored tokens are inputs; iiGUI owns the visual model and host.
template<class NativeButtonModel>struct AlertButtonModelPolicy final {
    using Options=AlertButtonConfiguration;using Appearance=AlertButtonAppearance;using Tone=ButtonTone;
    using Color=StyleColor;using Button=NativeButtonModel;
    [[nodiscard]] static bool valid_color(Color value)noexcept{return valid_color_value(value);}
    [[nodiscard]] static bool valid_extra(const Options&)noexcept{return true;}
    [[nodiscard]] static Color primary_reference(const Options&)noexcept{return figma_defaults::accent_blue_default;}
    [[nodiscard]] static Color dialog_background(const Options&)noexcept{return {0,0,0,0};}
    [[nodiscard]] static Color dialog_hover(const Options&)noexcept{return {1,1,1,.05};}
    [[nodiscard]] static Color dialog_pressed(const Options&)noexcept{return {1,1,1,.09};}
    [[nodiscard]] static double padding(const Options& o)noexcept{return o.dialog_style?16:8;}
    [[nodiscard]] static double radius(const Options& o)noexcept{return o.dialog_style?16:4;}
    [[nodiscard]] static double border_width(const Options&)noexcept{return 1;}
    [[nodiscard]] static double hover_factor(const Options&)noexcept{return 1.12;}
    [[nodiscard]] static double pressed_factor(const Options&)noexcept{return 1.2;}
    [[nodiscard]] static double normal_hover_factor(const Options&)noexcept{return 1.08;}
};
template<class NativeModel,class NativeButton,class Decoder,class Widget>struct AlertButtonHostPolicy final {
    using Model=NativeModel;using Options=AlertButtonConfiguration;using ButtonInput=ButtonInputValue<typename Decoder::Image>;
    using Button=NativeButton;using Callbacks=NativeAlertButtonCallbacksValue<Widget>;using Method=ButtonMethodValue<ButtonEventValue<Widget>>;
};
template<template<class>class NativeButtonModel,template<class>class NativeButtonHost,
         template<class>class NativeModel,template<class>class NativeHost,class Decoder,class Widget>
using AlertButtonBinding=NativeHost<AlertButtonHostPolicy<NativeModel<AlertButtonModelPolicy<NativeButtonModel<ButtonHostModelPolicy>>>,
    ButtonBinding<NativeButtonModel,NativeButtonHost,Decoder,Widget>,Decoder,Widget>>;
} // namespace lvui::native
