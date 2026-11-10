#pragma once
#include <LVUI/Button.h>
#include <LVUI/ButtonMotionDefaults.generated.h>
#include <LVUI/NativeButtonMethods.h>
#include <LVUI/Motion.h>
#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeStylePort.h>
#include <iiSDKCore/Runtime/Execution.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {
// Applications provide the graphics types and compose the native host.
template<class Image>struct ButtonInputValue final {
    std::shared_ptr<const Image> icon;
    ButtonConfiguration configuration;
    std::string icon_name;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision=0;
    friend bool operator==(const ButtonInputValue&,const ButtonInputValue&)=default;
};
struct NativeButtonEvents final {
    ii::runtime::Task pressed,released,canceled;
};
inline ButtonTone button_tone(ButtonTone value) noexcept { return value; }
inline ButtonSelectionStyle button_selection_style(ButtonSelectionStyle value) noexcept { return value; }
template<class Color>inline StyleColor button_color(const Color& value) noexcept { return {value.red,value.green,value.blue,value.alpha}; }
template<class Tone> requires (!std::same_as<Tone,ButtonTone> && requires {
    Tone::primary;Tone::normal;Tone::borderless;Tone::destructive;Tone::disabled;
})
inline ButtonTone button_tone(Tone value) noexcept {
    switch(value){
    case Tone::primary:return ButtonTone::primary;
    case Tone::normal:return ButtonTone::normal;
    case Tone::borderless:return ButtonTone::borderless;
    case Tone::destructive:return ButtonTone::destructive;
    case Tone::disabled:return ButtonTone::disabled;
    }
    return static_cast<ButtonTone>(-1);
}

// The application owns its token and icon sources. iiGUI owns the button
// model, input state, geometry, motion, drawing and retained widget lifetime.
struct ButtonHostModelPolicy final {
    using Configuration=ButtonConfiguration; using Interaction=ButtonInteraction;
    using Visual=ButtonVisual; using Geometry=ButtonGeometry; using Tone=ButtonTone;
    using SelectionStyle=ButtonSelectionStyle; using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view text) noexcept {
        return valid_accessibility_text(text);
    }
};
template<class NativeModel,class Decoder,class Widget>
struct ButtonHostPolicy final {
    using Model=NativeModel;
    using Input=ButtonInputValue<typename Decoder::Image>; using Tone=ButtonTone; using Rect=ButtonRect; using Color=StyleColor;
    using IconCache=NativeIconCache<Decoder>; using Events=NativeButtonEvents; using Event=ButtonEventValue<Widget>;
    using Method=ButtonMethodValue<Event>; using MethodRegistry=ButtonMethodRegistryValue<Event>; using TonePort=BorderlessTonePort;
    [[nodiscard]] static bool method_active(const Widget& root) {
        return button_method_active(root);
    }
    [[nodiscard]] static ii::core::Result<void> invoke_methods(const Widget& root,
            const MethodRegistry& methods,bool enabled,std::optional<Tone> tone,std::string_view trigger) {
        return invoke_button_methods(root,methods,enabled,tone,trigger);
    }
    [[nodiscard]] static auto hover_duration(const Input&) {return button_motion_defaults::hover;}
    [[nodiscard]] static auto press_duration(const Input&) {return button_motion_defaults::press;}
    [[nodiscard]] static auto release_duration(const Input&) {return button_motion_defaults::release;}
    [[nodiscard]] static auto post_release_duration(const Input&) {return button_motion_defaults::post_release;}
    [[nodiscard]] static std::chrono::milliseconds programmatic_duration(const Input&) {return std::chrono::milliseconds{130};}
    [[nodiscard]] static std::chrono::milliseconds focus_opacity_duration(const Input&) {return std::chrono::milliseconds{160};}
    [[nodiscard]] static std::chrono::milliseconds focus_scale_duration(const Input&) {return std::chrono::milliseconds{360};}
};
template<template<class>class NativeModel,template<class>class NativeHost,class Decoder,class Widget>
using ButtonBinding=NativeHost<ButtonHostPolicy<NativeModel<ButtonHostModelPolicy>,Decoder,Widget>>;

} // namespace lvui::native
