#pragma once
#include <LVUI/HierarchyItem.h>
#include <LVUI/NativeIconCache.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor hierarchy_item_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
template<class Image>struct HierarchyItemInputValue final {
 HierarchyItemConfiguration configuration;std::shared_ptr<const Image> icon;std::shared_ptr<const IconSource> icon_source;std::int64_t icon_revision=0;
 friend bool operator==(const HierarchyItemInputValue&,const HierarchyItemInputValue&)=default;
};
struct NativeHierarchyItemCallbacks final {std::function<void()> activated;std::function<void(bool)> expansion_changed;};
template<class NativeModel,class Decoder>struct HierarchyItemHostPolicy final {
 using Input=HierarchyItemInputValue<typename Decoder::Image>;using Model=NativeModel;using Rect=HierarchyItemRect;using Color=StyleColor;
 using Callbacks=NativeHierarchyItemCallbacks;using IconCache=NativeIconCache<Decoder>;
 inline static constexpr double label_font_size=13,label_line_height=16,count_font_size=11,count_line_height=13;
 inline static constexpr unsigned label_font_weight=500,count_font_weight=400;
 inline static constexpr double disabled_opacity=.35,focus_stroke=1.5,chevron_focus_radius=4;
 inline static constexpr std::string_view expanded_icon="generalchevronDown",collapsed_icon="generalchevronRight";
};
template<template<class>class Host,template<class>class NativeModel,class Decoder>
using HierarchyItemBinding=Host<HierarchyItemHostPolicy<HierarchyItemModelBinding<NativeModel>,Decoder>>;
} // namespace lvui::native
