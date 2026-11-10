#pragma once
#include <LVUI/ListToolbar.h>
#include <LVUI/NativeIconCache.h>

#include <array>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace lvui::native {

template<class Color>[[nodiscard]] inline StyleColor list_toolbar_color(const Color& value) noexcept {
    return {value.red, value.green, value.blue, value.alpha};
}
struct ListToolbarInput final {
    ListToolbarConfiguration configuration;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision = 0;
    friend bool operator==(const ListToolbarInput&, const ListToolbarInput&) = default;
};
struct NativeListToolbarCallbacks final {
    std::function<void(std::size_t, std::string)> clicked;
};

// LVUI provides existing value types and design assets; iiGUI owns the widget.
struct ListToolbarModelPolicy final {
    using Configuration=ListToolbarConfiguration;using Geometry=ListToolbarGeometry;using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<class NativeModel,class Decoder>
struct ListToolbarHostPolicy final {
    using Input=ListToolbarInput;using Model=NativeModel;
    using Rect=ListToolbarRect;using Color=StyleColor;using IconCache=NativeIconCache<Decoder>;using Callbacks=NativeListToolbarCallbacks;
};
template<template<class>class NativeModel,template<class>class NativeHost,class Decoder>
using ListToolbarBinding=NativeHost<ListToolbarHostPolicy<NativeModel<ListToolbarModelPolicy>,Decoder>>;
} // namespace lvui::native
