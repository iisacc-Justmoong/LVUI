#pragma once
#include <LVUI/NativeIconCache.h>
#include <LVUI/ToolbarButton.h>

#include <concepts>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {

template<class Color>[[nodiscard]] inline StyleColor toolbar_button_color(const Color& value) noexcept {
    return {value.red, value.green, value.blue, value.alpha};
}
[[nodiscard]] inline ToolbarButtonId toolbar_button_id(ToolbarButtonId value) { return value; }
[[nodiscard]] inline ToolbarButtonId toolbar_button_id(std::string value) { return value; }
[[nodiscard]] inline ToolbarButtonId toolbar_button_id(std::string_view value) { return std::string(value); }
[[nodiscard]] inline ToolbarButtonId toolbar_button_id(const char* value) { return std::string(value ? value : ""); }
template<std::integral T> requires (!std::same_as<T, bool>)
[[nodiscard]] inline ToolbarButtonId toolbar_button_id(T value) {
    if (!std::in_range<std::int64_t>(value)) throw std::out_of_range("Toolbar button ID is out of range");
    return std::int64_t(value);
}
[[nodiscard]] inline ButtonTone toolbar_button_tone(ButtonTone value) noexcept { return value; }
template<class Tone> requires (!std::same_as<Tone,ButtonTone> && requires {
    Tone::primary;Tone::normal;Tone::borderless;Tone::destructive;Tone::disabled;
})
[[nodiscard]] inline ButtonTone toolbar_button_tone(Tone value) noexcept {
    switch (value) {
    case Tone::primary: return ButtonTone::primary;
    case Tone::normal: return ButtonTone::normal;
    case Tone::borderless: return ButtonTone::borderless;
    case Tone::destructive: return ButtonTone::destructive;
    case Tone::disabled: return ButtonTone::disabled;
    }
    return static_cast<ButtonTone>(-1);
}
template<class Image>struct ToolbarButtonInputValue final {
    ToolbarButtonConfiguration configuration;
    std::string icon_name;
    std::shared_ptr<const Image> icon;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision = 0;
    friend bool operator==(const ToolbarButtonInputValue&, const ToolbarButtonInputValue&) = default;
};
struct NativeToolbarButtonCallbacks final {
    std::function<void()> clicked;
    std::function<void(const ToolbarButtonId&)> triggered;
};

// LVUI supplies existing value types, icon assets and authored tokens. iiGUI owns the widget.
struct ToolbarButtonModelPolicy final {
    using Configuration=ToolbarButtonConfiguration;using Geometry=ToolbarButtonGeometry;using Visual=ToolbarButtonVisual;
    using Interaction=ButtonInteraction;using Tone=ButtonTone;using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept {return valid_accessibility_text(value);}
};
template<class NativeModel,class Decoder>
struct ToolbarButtonHostPolicy final {
    using Input=ToolbarButtonInputValue<typename Decoder::Image>;using Configuration=ToolbarButtonConfiguration;
    using Model=NativeModel;
    using Rect=ToolbarButtonRect;using Tone=ButtonTone;using Color=StyleColor;using IconCache=NativeIconCache<Decoder>;
    using Callbacks=NativeToolbarButtonCallbacks;
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept {return valid_accessibility_text(value);}
};
template<template<class>class NativeModel,template<class>class NativeHost,class Decoder>
using ToolbarButtonBinding=NativeHost<ToolbarButtonHostPolicy<NativeModel<ToolbarButtonModelPolicy>,Decoder>>;
} // namespace lvui::native
