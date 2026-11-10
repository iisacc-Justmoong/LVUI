#pragma once
#include <LVUI/ColorSurface.h>
#include <cstdint>
#include <memory>
namespace lvui::native {
struct ColorSurfaceInput final {ColorSurfaceConfiguration configuration;std::shared_ptr<ColorSurfaceSource> source;std::int64_t source_revision=0;
 friend bool operator==(const ColorSurfaceInput&,const ColorSurfaceInput&)=default;};
template<class NativeModel>struct ColorSurfaceHostPolicy final {
 using Model=NativeModel;using Input=ColorSurfaceInput;using Frame=ColorSurfaceFrame;using Kind=ColorSurfaceKind;
 using ErrorCode=ColorSurfaceErrorCode;using Color=StyleColor;
};
template<template<class>class NativeModel,template<class>class NativeHost>
using ColorSurfaceBinding=NativeHost<ColorSurfaceHostPolicy<NativeModel<ColorSurfaceModelPolicy>>>;
} // namespace lvui::native
