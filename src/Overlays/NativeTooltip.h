#pragma once
#include <LVUI/NativeTooltipTarget.h>
#include <LVUI/NativeTooltipScheduler.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
namespace lvui::native {
[[nodiscard]] inline std::chrono::milliseconds tooltip_duration(double milliseconds)noexcept {
    if(!std::isfinite(milliseconds)||milliseconds<0||milliseconds>86400000||std::trunc(milliseconds)!=milliseconds)
        return std::chrono::milliseconds{-1};
    return std::chrono::milliseconds{static_cast<std::int64_t>(milliseconds)};
}
[[nodiscard]] inline TooltipPlacement tooltip_placement(TooltipPlacement value)noexcept{return value;}
[[nodiscard]] inline TooltipRect tooltip_available_rect(TooltipRect value)noexcept{return value;}
template<class Rect> requires requires(const Rect& value){value.x;value.y;value.width;value.height;}
[[nodiscard]] inline TooltipRect tooltip_available_rect(const Rect& value)noexcept{
    return {value.x,value.y,value.width,value.height};
}
template<class T>
[[nodiscard]] inline std::optional<TooltipRect> tooltip_available_rect(const std::optional<T>& value)noexcept{
    if(!value)return std::nullopt;
    return tooltip_available_rect(*value);
}
template<class Motion,class NativeModel,class NativeController,class NativePresentation,class NativeScheduler,class NativeTarget>
struct TooltipFacadePolicy final {
    using Model=NativeModel;using Controller=NativeController;using Presentation=NativePresentation;using Scheduler=NativeScheduler;using Target=NativeTarget;
    using Configuration=TooltipConfiguration;using Style=TooltipPresentationStyleValue<Motion>;using Options=TooltipTargetOptions;
    using Callbacks=TooltipCallbacks;using Input=TooltipInputValue<Motion>;using Diagnostic=TooltipDiagnostic;using Snapshot=TooltipSnapshot;
};
template<template<class>class Host,class Motion,class Model,class Controller,class Presentation,class Scheduler,class Target>
using TooltipBinding=Host<TooltipFacadePolicy<Motion,Model,Controller,Presentation,Scheduler,Target>>;
} // namespace lvui::native
