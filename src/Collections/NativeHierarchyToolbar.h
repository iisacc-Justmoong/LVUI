#pragma once
#include <LVUI/HierarchyToolbarLayout.h>
#include <LVUI/NativeToolbarButton.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace lvui::native {
template<class NativeButtonModel>
struct HierarchyToolbarLayoutPolicy final {
    using Appearance=HierarchyToolbarAppearance;using Snapshot=HierarchyToolbarSnapshot;
    using Geometry=HierarchyToolbarGeometry;using Color=StyleColor;
    using ButtonConfiguration=ToolbarButtonConfiguration;using ButtonModel=NativeButtonModel;
    [[nodiscard]] static double slot_padding(const Appearance& value)noexcept{return value.slot_padding;}
    [[nodiscard]] static double icon_text_spacing(const Appearance& value)noexcept{return value.icon_text_spacing;}
};
template<template<class>class Layout,class ButtonModel>
using HierarchyToolbarLayoutBinding=Layout<HierarchyToolbarLayoutPolicy<ButtonModel>>;

template<class Color> [[nodiscard]] inline StyleColor hierarchy_toolbar_color(const Color& value) noexcept {
    return {value.red, value.green, value.blue, value.alpha};
}
[[nodiscard]] inline std::vector<HierarchyToolbarItem> hierarchy_toolbar_items(std::vector<HierarchyToolbarItem> value) { return value; }
[[nodiscard]] inline std::vector<HierarchyToolbarItem> hierarchy_toolbar_items(const std::shared_ptr<const std::vector<HierarchyToolbarItem>>& value) {
    return value ? *value : std::vector<HierarchyToolbarItem>{};
}
[[nodiscard]] inline std::vector<HierarchyToolbarItem> hierarchy_toolbar_items(const std::shared_ptr<std::vector<HierarchyToolbarItem>>& value) {
    return value ? *value : std::vector<HierarchyToolbarItem>{};
}
template<class T> [[nodiscard]] inline std::optional<ToolbarButtonId> hierarchy_toolbar_active(T&& value) {
    if constexpr (requires { value.has_value(); *value; }) {
        if (!value) return std::nullopt; return toolbar_button_id(*value);
    } else return toolbar_button_id(std::forward<T>(value));
}
struct HierarchyToolbarInput final {
    std::vector<HierarchyToolbarItem> items;
    std::optional<ToolbarButtonId> active_id;
    HierarchyToolbarAppearance appearance;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision = 0;
    friend bool operator==(const HierarchyToolbarInput&, const HierarchyToolbarInput&) = default;
};
struct NativeHierarchyToolbarCallbacks final {
    std::function<void(const HierarchyToolbarSelection&)> active_changed;
    std::function<void(const HierarchyToolbarSelection&)> triggered;
};

template<class NativeModel,class NativeLayout,class NativeButtonModel,class NativeButton,class ImageType,class Decoder>
struct HierarchyToolbarHostPolicy final {
    using Input=HierarchyToolbarInput;using Model=NativeModel;using Layout=NativeLayout;
    using ButtonModel=NativeButtonModel;using Button=NativeButton;using ButtonInput=ToolbarButtonInputValue<ImageType>;
    using Image=ImageType;using ButtonTone=lvui::ButtonTone;using IconCache=NativeIconCache<Decoder>;
    using Id=ToolbarButtonId;using Snapshot=HierarchyToolbarSnapshot;using Selection=HierarchyToolbarSelection;
    using Error=HierarchyToolbarError;using ErrorCode=HierarchyToolbarErrorCode;using Direction=HierarchyToolbarDirection;
    using Geometry=HierarchyToolbarGeometry;using Rect=HierarchyToolbarRect;using Color=StyleColor;
    using Callbacks=NativeHierarchyToolbarCallbacks;
};
template<template<class>class Host,class Model,class Layout,class ButtonModel,class Button,class Image,class Decoder>
using HierarchyToolbarBinding=Host<HierarchyToolbarHostPolicy<Model,Layout,ButtonModel,Button,Image,Decoder>>;
} // namespace lvui::native
