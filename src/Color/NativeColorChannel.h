#pragma once
#include <LVUI/ColorChannelSurface.h>
#include <LVUI/NativeColorSurface.h>
#include <LVUI/NativeInputField.h>
#include <cstdint>
#include <memory>
namespace lvui::native {
struct ColorChannelInput final {
 std::shared_ptr<ColorChannelsSource> source;ColorChannel channel=ColorChannel::red;
 std::int64_t source_revision=0;double width=220;bool enabled=true;
 friend bool operator==(const ColorChannelInput&,const ColorChannelInput&)=default;
};
template<class NativeRamp,class NativeSurface,class NativeField>struct ColorChannelHostPolicy final {
 using Input=ColorChannelInput;using Frame=ColorChannelFrame;using Channel=ColorChannel;using Channels=ColorChannels;
 using SurfaceError=ColorSurfaceError;using SurfaceErrorCode=ColorSurfaceErrorCode;using RampSource=NativeRamp;
 using Surface=NativeSurface;using SurfaceInput=ColorSurfaceInput;using SurfaceKind=ColorSurfaceKind;
 using Field=NativeField;using FieldConfiguration=InputFieldConfiguration;using FieldCallbacks=NativeInputFieldCallbacks;using Color=StyleColor;
};
template<template<class>class NativeHost,class NativeRamp,class NativeSurface,class NativeField>
using ColorChannelBinding=NativeHost<ColorChannelHostPolicy<NativeRamp,NativeSurface,NativeField>>;
} // namespace lvui::native
