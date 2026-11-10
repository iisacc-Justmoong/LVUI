#pragma once
#include <LVUI/DropdownButton.h>
#include <LVUI/NativeButton.h>
#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeStylePort.h>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace lvui::native {
template<class NativeButtonModel>struct DropdownModelPolicy final {
    using Configuration=DropdownConfiguration;using Geometry=DropdownGeometry;using ButtonConfig=ButtonConfiguration;
    using Tone=ButtonTone;using Visual=ButtonVisual;using Interaction=ButtonInteraction;using Color=StyleColor;
    using Button=NativeButtonModel;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
[[nodiscard]] inline ButtonTone dropdown_tone(ButtonTone value)noexcept{return value;}
template<class Tone> requires (!std::same_as<Tone,ButtonTone>)
[[nodiscard]] inline ButtonTone dropdown_tone(Tone value)noexcept{return button_tone(value);}
template<class Color>[[nodiscard]] inline StyleColor dropdown_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
template<class Image>struct DropdownInputValue final {
    DropdownConfiguration configuration;
    std::shared_ptr<const Image> icon;
    std::string icon_name;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision=0;
    friend bool operator==(const DropdownInputValue&,const DropdownInputValue&)=default;
};
struct NativeDropdownCallbacks final {std::function<void()> clicked,pressed,released,canceled;};
template<class NativeModel,class Decoder,class Widget>struct DropdownHostPolicy final {
    using Model=NativeModel;using Input=DropdownInputValue<typename Decoder::Image>;using Tone=ButtonTone;using Rect=ButtonRect;using Color=StyleColor;
    using IconCache=NativeIconCache<Decoder>;using Callbacks=NativeDropdownCallbacks;
    using Event=ButtonEventValue<Widget>;using Method=ButtonMethodValue<Event>;using MethodRegistry=ButtonMethodRegistryValue<Event>;using TonePort=BorderlessTonePort;
    [[nodiscard]] static bool valid_input(const Input& input)noexcept {
        return !(input.icon&&!input.icon_name.empty())&&(!input.icon_source||!input.icon_name.empty())&&input.icon_revision>=0;
    }
    [[nodiscard]] static std::uint64_t source_revision(const Input& input)noexcept {
        return !input.icon_name.empty()&&input.icon_source?input.icon_source->revision():0;
    }
    [[nodiscard]] static bool method_active(const Widget& root){return button_method_active(root);}
    [[nodiscard]] static ii::core::Result<void> invoke_methods(const Widget& root,const MethodRegistry& methods,
            bool enabled,std::optional<Tone> tone,std::string_view trigger){return invoke_button_methods(root,methods,enabled,tone,trigger);}
};
template<template<class>class NativeButtonModel,template<class>class NativeModel,template<class>class NativeHost,class Decoder,class Widget>
using DropdownBinding=NativeHost<DropdownHostPolicy<NativeModel<DropdownModelPolicy<NativeButtonModel<ButtonHostModelPolicy>>>,Decoder,Widget>>;
} // namespace lvui::native
