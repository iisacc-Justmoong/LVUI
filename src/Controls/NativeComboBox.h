#pragma once
#include <LVUI/ComboBox.h>
#include <LVUI/NativeButton.h>
#include <concepts>
#include <functional>
#include <string_view>
namespace lvui::native {
[[nodiscard]] inline ButtonTone combo_box_tone(ButtonTone value)noexcept{return value;}
template<class Tone> requires (!std::same_as<Tone,ButtonTone>)
[[nodiscard]] inline ButtonTone combo_box_tone(Tone value)noexcept{return button_tone(value);}
[[nodiscard]] inline StepperArrow combo_box_arrow(StepperArrow value)noexcept{return value;}
template<class Arrow> requires (!std::same_as<Arrow,StepperArrow>)
[[nodiscard]] inline StepperArrow combo_box_arrow(Arrow value)noexcept{
    switch(value){case Arrow::up_down:return StepperArrow::up_down;case Arrow::up:return StepperArrow::up;case Arrow::down:return StepperArrow::down;}
    return static_cast<StepperArrow>(-1);
}
template<class Color>[[nodiscard]] inline StyleColor combo_box_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
struct NativeComboBoxCallbacks final {std::function<void()> clicked,pressed,released,canceled;};
template<class NativeModel,class Widget>struct ComboBoxHostPolicy final {
    using Model=NativeModel;using Options=ComboBoxConfiguration;using Callbacks=NativeComboBoxCallbacks;
    using Event=ButtonEventValue<Widget>;using Method=ButtonMethodValue<Event>;using Registry=ButtonMethodRegistryValue<Event>;
    using Color=StyleColor;using Rect=ButtonRect;
    [[nodiscard]] static bool active(const Widget& root){return button_method_active(root);}
    [[nodiscard]] static ii::core::Result<void> invoke(const Widget& root,const Registry& methods,bool enabled,ButtonTone tone,std::string_view trigger){return invoke_button_methods(root,methods,enabled,tone,trigger);}
};
template<template<class>class NativeModel,template<class>class NativeHost,class Widget>
using ComboBoxBinding=NativeHost<ComboBoxHostPolicy<NativeModel<ComboBoxModelPolicy>,Widget>>;
} // namespace lvui::native
