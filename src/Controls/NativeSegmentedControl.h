#pragma once
#include <LVUI/SegmentedControl.h>
#include <LVUI/NativeStylePort.h>
#include <functional>
#include <string_view>

namespace lvui::native {
[[nodiscard]] inline SegmentedShape segmented_shape(SegmentedShape value)noexcept{return value;}
template<class Color>[[nodiscard]] inline StyleColor segmented_color(const Color& value)noexcept{
    return {value.red,value.green,value.blue,value.alpha};
}
using SegmentedToneSink=BorderlessTonePort;
using SegmentedRegistrationPort=std::function<ii::core::Result<void>(SegmentedToneSink)>;
struct SegmentedControlModelPolicy final {
    using Configuration=SegmentedControlConfiguration;using Geometry=SegmentedControlGeometry;
    using Shape=SegmentedShape;using Orientation=SegmentedOrientation;using Color=StyleColor;
};
template<class NativeModel,class NativeMethod,class NativeRegistry,class Invoker>
struct SegmentedControlHostPolicy final {
    using Model=NativeModel;using Configuration=SegmentedControlConfiguration;
    using Orientation=SegmentedOrientation;using Color=StyleColor;
    using Method=NativeMethod;using MethodRegistry=NativeRegistry;using ToneSink=SegmentedToneSink;
    template<class Widget>[[nodiscard]] static ii::core::Result<void> invoke_methods(
            const Widget& root,const MethodRegistry& methods,bool enabled,std::string_view trigger){
        return Invoker::invoke(root,methods,enabled,trigger);
    }
};
template<template<class>class NativeModel,template<class>class NativeHost,class Method,class Registry,class Invoker>
using SegmentedControlBinding=NativeHost<SegmentedControlHostPolicy<NativeModel<SegmentedControlModelPolicy>,Method,Registry,Invoker>>;
} // namespace lvui::native
