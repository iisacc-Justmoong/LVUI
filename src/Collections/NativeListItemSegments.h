#pragma once
#include <LVUI/ListItemSegments.h>
#include <LVUI/NativeButton.h>
#include <LVUI/NativeSegmentedControl.h>
#include <LVUI/NativeVisual.h>
#include <LVUI/WindowAccent.h>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>
namespace lvui::native {
[[nodiscard]] inline std::vector<ListItemSegment> list_item_segments(std::vector<ListItemSegment> options){return options;}
[[nodiscard]] inline std::vector<ListItemSegment> list_item_segments(const std::vector<std::string>& options){
 std::vector<ListItemSegment> result;result.reserve(options.size());for(const auto& text:options)result.push_back({text,true,{}});return result;}
struct ListItemSegmentsInput final {ListItemSegmentsConfiguration configuration;std::optional<WindowAccentPalette> accent;bool visible=true;
 friend bool operator==(const ListItemSegmentsInput&,const ListItemSegmentsInput&)=default;};
template<class NativeModel,class Button,class Group,class Image>
struct ListItemSegmentsHostPolicy final {
 using Model=NativeModel;using Input=ListItemSegmentsInput;using NativeButton=Button;using NativeSegmentedControl=Group;
 [[nodiscard]] static bool valid_accent(const std::optional<WindowAccentPalette>& accent)noexcept{
  return !accent||valid_window_accent_palette(*accent);}
 [[nodiscard]] static SegmentedControlConfiguration group_input(const ListItemSegmentsConfiguration& c){
  SegmentedControlConfiguration value;value.horizontal_padding=c.horizontal_padding;value.vertical_padding=c.vertical_padding;
  value.spacing=c.spacing;value.enabled=c.enabled;value.force_borderless_tone=false;return value;}
 [[nodiscard]] static ButtonInputValue<Image> button_input(const ListItemSegmentsConfiguration& c,std::size_t index,
         const std::optional<WindowAccentPalette>& accent){
  ButtonInputValue<Image> input;const auto& option=c.options[index];auto& b=input.configuration;
  b.text=native_display_text(option.text);b.accessibility_name=b.text;b.width=c.segment_width;b.height=c.segment_height;
  if(accent){b.palette.primary=accent->primary;b.palette.primary_hover=accent->hover;b.palette.primary_pressed=accent->pressed;
   b.palette.normal_pressed=accent->subtle;b.palette.selected=accent->selected;}
  b.enabled=c.enabled&&option.enabled;b.tone=ButtonTone::borderless;b.checkable=true;b.checked=int(index)==c.selected_index;return input;}
};
template<template<class>class NativeModel,template<class>class NativeHost,class Button,class Group,class Image>
using ListItemSegmentsBinding=NativeHost<ListItemSegmentsHostPolicy<NativeModel<ListItemSegmentsModelPolicy>,Button,Group,Image>>;
} // namespace lvui::native
