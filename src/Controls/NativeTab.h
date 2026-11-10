#pragma once
#include <LVUI/NativeIconCache.h>
#include <LVUI/Tab.h>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
namespace lvui::native {
[[nodiscard]] inline TabStyle tab_style(TabStyle value)noexcept{return value;}
template<class Style> requires (!std::same_as<Style,TabStyle>)
[[nodiscard]] inline TabStyle tab_style(Style value)noexcept{
 switch(value){case Style::underline:return TabStyle::underline;case Style::surface:return TabStyle::surface;}return static_cast<TabStyle>(-1);}
struct TabInput final {
    TabConfiguration configuration;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision=0;
    bool visible=true;
    friend bool operator==(const TabInput&,const TabInput&)=default;
};
// A non-owning parent connection. Presentation is an override, never a write to
// the tab's authored values. The activation hook returns false on failure.
struct TabPresentation final {
    std::optional<bool> selected;std::optional<double> width;
    friend bool operator==(const TabPresentation&,const TabPresentation&)=default;
};
struct TabAvailability final {bool enabled=true,visible=true;};
template<class Widget,class Snapshot>struct TabGroupPortValue final {
    std::function<ii::core::Result<void>(TabPresentation)> present;
    std::function<ii::core::Result<void>(std::function<bool()>)> activated;
    std::function<ii::core::Result<void>(std::function<void()>)> changed;
    std::function<ii::core::Result<TabAvailability>()> availability;
    std::function<ii::core::Result<double>()> minimum_width;
    std::function<ii::core::Result<Snapshot>()> snapshot;
    std::function<ii::core::Result<void>()> focus;
    std::function<bool()> retired;
    std::function<ii::core::Result<Widget>()> widget;
    std::vector<Widget> layout_groups;
};
template<class NativeModel,class Decoder,class Widget,class Snapshot>struct TabHostPolicy final {
 using Model=NativeModel;using Input=TabInput;using Presentation=TabPresentation;using GroupPort=TabGroupPortValue<Widget,Snapshot>;
 using Availability=TabAvailability;using Rect=TabRect;using Color=StyleColor;using IconCache=NativeIconCache<Decoder>;
 [[nodiscard]] static std::uint64_t source_revision(const Input& value)noexcept{return value.icon_source?value.icon_source->revision():0;}
};
template<template<class>class NativeModel,template<class>class NativeHost,class Decoder,class Widget,class Snapshot>
using TabBinding=NativeHost<TabHostPolicy<NativeModel<TabModelPolicy>,Decoder,Widget,Snapshot>>;
} // namespace lvui::native
