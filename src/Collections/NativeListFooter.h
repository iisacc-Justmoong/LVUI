#pragma once
#include <LVUI/NativeIconCache.h>
#include <LVUI/ListFooter.h>
#include <LVUI/NativeToolbarButton.h>

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

template<class Color>[[nodiscard]] inline StyleColor list_footer_color(const Color& value) noexcept {
    return {value.red, value.green, value.blue, value.alpha};
}
[[nodiscard]] inline ListFooterSlot list_footer_slot(ListFooterSlot value) { return value; }
[[nodiscard]] inline ListFooterSlot list_footer_slot(const std::shared_ptr<const ListFooterSlot>& value) {
    if (value) return *value; ListFooterSlot result; result.visible = false; return result;
}
[[nodiscard]] inline ListFooterSlot list_footer_slot(const std::shared_ptr<ListFooterSlot>& value) {
    return list_footer_slot(std::shared_ptr<const ListFooterSlot>(value));
}
[[nodiscard]] inline ListFooterSlot list_footer_slot(const std::optional<ListFooterSlot>& value) {
    if (value) return *value; ListFooterSlot result; result.visible = false; return result;
}
struct ListFooterInput final {
    ListFooterConfiguration configuration;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision = 0;
    friend bool operator==(const ListFooterInput&, const ListFooterInput&) = default;
};
struct NativeListFooterCallbacks final {
    std::function<void(std::size_t, const ListFooterSlot&)> clicked;
};

// The policy supplies LVUI values and authored assets; iiGUI owns layout and lifetime.
template<class NativeButtonModel>struct ListFooterModelPolicy final {
    using Configuration=ListFooterConfiguration;using Geometry=ListFooterGeometry;
    using ButtonConfiguration=ToolbarButtonConfiguration;using ButtonModel=NativeButtonModel;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
    [[nodiscard]] static bool valid_extra(const Configuration&)noexcept{return true;}
    [[nodiscard]] static double gap(const Configuration&)noexcept{return 4;}
    [[nodiscard]] static double slot_padding(const Configuration&)noexcept{return 4;}
    [[nodiscard]] static double menu_width(const Configuration&)noexcept{return 18;}
};
template<class NativeModel,class NativeButtonModel,class NativeButton,class Decoder>
struct ListFooterHostPolicy final {
    using Input=ListFooterInput;using Model=NativeModel;using Rect=ListFooterRect;
    using ButtonModel=NativeButtonModel;
    using ButtonInput=ToolbarButtonInputValue<typename Decoder::Image>;using Button=NativeButton;
    using IconCache=NativeIconCache<Decoder>;using Callbacks=NativeListFooterCallbacks;
    [[nodiscard]] static std::string_view indicator(const ListFooterConfiguration&)noexcept{return "⌄";}
};
template<template<class>class NativeModel,template<class>class NativeHost,
         template<class>class NativeButtonModel,template<class>class NativeButtonHost,class Decoder>
using ListFooterBinding=NativeHost<ListFooterHostPolicy<NativeModel<ListFooterModelPolicy<NativeButtonModel<ToolbarButtonModelPolicy>>>,
    NativeButtonModel<ToolbarButtonModelPolicy>,ToolbarButtonBinding<NativeButtonModel,NativeButtonHost,Decoder>,Decoder>>;
} // namespace lvui::native
