#pragma once
#include <LVUI/Stepper.h>
#include <LVUI/NativeButton.h>
#include <concepts>
#include <functional>
#include <optional>
#include <string_view>

namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor stepper_color(const Color& value) noexcept { return {value.red,value.green,value.blue,value.alpha}; }
[[nodiscard]] inline ButtonTone stepper_tone(ButtonTone value) noexcept { return value; }
template<class Tone> requires (!std::same_as<Tone,ButtonTone>)
[[nodiscard]] inline ButtonTone stepper_tone(Tone value) noexcept { return button_tone(value); }
[[nodiscard]] inline StepperArrow stepper_arrow(StepperArrow value) noexcept { return value; }
template<class Arrow> requires (!std::same_as<Arrow,StepperArrow> && requires {Arrow::up_down;Arrow::up;Arrow::down;})
[[nodiscard]] inline StepperArrow stepper_arrow(Arrow value) noexcept {
    switch(value) {
    case Arrow::up_down:return StepperArrow::up_down;
    case Arrow::up:return StepperArrow::up;
    case Arrow::down:return StepperArrow::down;
    }
    return static_cast<StepperArrow>(-1);
}
struct NativeStepperCallbacks final {
    std::function<void(int)> stepped;
    std::function<void()> clicked,pressed,released,canceled;
};
template<class NativeButtonModel>struct StepperModelPolicy final {
    using Configuration=StepperConfiguration;using Arrow=StepperArrow;using Tone=ButtonTone;using Color=StyleColor;
    using Button=NativeButtonModel;using ButtonConfig=ButtonConfiguration;
    using Interaction=ButtonInteraction;using Visual=ButtonVisual;
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept { return valid_accessibility_text(value); }
};
template<class NativeModel,class Widget>struct StepperHostPolicy final {
    using Model=NativeModel;using Configuration=StepperConfiguration;using Arrow=StepperArrow;
    using Tone=ButtonTone;using Color=StyleColor;using Callbacks=NativeStepperCallbacks;
    using Event=ButtonEventValue<Widget>;using Method=ButtonMethodValue<Event>;using MethodRegistry=ButtonMethodRegistryValue<Event>;
    [[nodiscard]] static bool method_active(const Widget& root) { return button_method_active(root); }
    [[nodiscard]] static ii::core::Result<void> invoke_methods(const Widget& root,const MethodRegistry& methods,
            bool enabled,std::optional<Tone> tone,std::string_view trigger) {
        return invoke_button_methods(root,methods,enabled,tone,trigger);
    }
};
template<template<class>class NativeButtonModel,template<class>class NativeModel,template<class>class NativeHost,class Widget>
using StepperBinding=NativeHost<StepperHostPolicy<NativeModel<StepperModelPolicy<NativeButtonModel<ButtonHostModelPolicy>>>,Widget>>;
} // namespace lvui::native
