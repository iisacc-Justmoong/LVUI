#pragma once
#include <LVUI/ColorChannels.h>
#include <cstdint>
namespace lvui {
struct ColorChannelSurfacePolicy final {
 using SurfaceSource=ColorSurfaceSource;using Source=ColorChannelsSource;using Channel=ColorChannel;using Kind=ColorSurfaceKind;
 using Point=ColorSurfacePoint;using Frame=ColorSurfaceFrame;using Error=ColorSurfaceError;using ErrorCode=ColorSurfaceErrorCode;
 using FrameResult=ColorSurfaceResult<Frame>;using EditResult=ColorSurfaceResult<std::uint64_t>;
 [[nodiscard]] static bool valid_channel(Channel channel)noexcept{return ColorChannels::valid_channel(channel);}
 [[nodiscard]] static bool valid_frame(const ColorChannelFrame& frame)noexcept{return ColorChannels::valid_frame(frame);}
};
template<template<class>class Implementation>using ColorChannelSurfaceBinding=Implementation<ColorChannelSurfacePolicy>;
} // namespace lvui
