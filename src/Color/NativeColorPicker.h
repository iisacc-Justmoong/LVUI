#pragma once
#include <LVUI/ColorPickerLayout.h>
#include <LVUI/NativeButton.h>
#include <LVUI/NativeColorChannel.h>
#include <LVUI/NativeColorPickerValues.h>
#include <cstdint>
#include <functional>
#include <memory>
namespace lvui::native {
struct ColorPickerInput final {
 ColorPickerConfiguration configuration;std::shared_ptr<ColorPickerSession> session;std::int64_t source_revision=0;bool preserve_recent_colors=false;
 friend bool operator==(const ColorPickerInput&,const ColorPickerInput&)=default;
};
template<class HostColor>struct NativeColorPickerCallbacksValue final {
 std::function<void(HostColor)> edited,accepted;std::function<void()> canceled,eyedropper_requested;
};
template<class NativeLayout,class NativeSurface,class NativeChannel,class NativeValues,class NativeButton,class Image,class HostColor>
struct ColorPickerHostPolicy final {
 using Input=ColorPickerInput;using Configuration=ColorPickerConfiguration;using Layout=ColorPickerLayout;using Rect=ColorPickerRect;
 using Snapshot=ColorSelectionSnapshot;using Color=StyleColor;using Picker=ColorPicker;using LayoutModel=NativeLayout;
 using Surface=NativeSurface;using ChannelView=NativeChannel;using Values=NativeValues;using Button=NativeButton;
 using SurfaceInput=ColorSurfaceInput;using ChannelInput=ColorChannelInput;using ValuesInput=ColorPickerValuesInput;
 using ButtonInput=ButtonInputValue<Image>;using SurfaceKind=ColorSurfaceKind;using Channel=ColorChannel;using ButtonTone=lvui::ButtonTone;
 using Callbacks=NativeColorPickerCallbacksValue<HostColor>;
 [[nodiscard]] static bool valid_color(Color value)noexcept{return valid_color_value(value);}
};
template<template<class>class Host,class Layout,class Surface,class Channel,class Values,class Button,class Image,class HostColor>
using ColorPickerBinding=Host<ColorPickerHostPolicy<Layout,Surface,Channel,Values,Button,Image,HostColor>>;
} // namespace lvui::native
