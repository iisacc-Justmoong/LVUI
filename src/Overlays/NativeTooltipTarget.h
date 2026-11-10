#pragma once
#include <LVUI/NativeTooltipPresentation.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstdint>
#include <optional>
namespace lvui::native {
struct TooltipTargetOptions final {
    std::optional<TooltipRect> available;
    std::optional<TooltipPoint> anchor;
    friend bool operator==(const TooltipTargetOptions&,const TooltipTargetOptions&)=default;
};
template<class Motion>
struct TooltipInputValue final {
    TooltipConfiguration configuration;
    TooltipPresentationStyleValue<Motion> style;
    TooltipTargetOptions target;
    friend bool operator==(const TooltipInputValue&,const TooltipInputValue&)=default;
};
struct TooltipDiagnostic final {
    std::uint64_t revision=0;
    std::optional<ii::core::Error> failure;
    friend bool operator==(const TooltipDiagnostic& a,const TooltipDiagnostic& b)noexcept{return a.revision==b.revision;}
};
template<class Motion,class NativeModel,class NativeController,class NativePresentation,class NativeScheduler>
struct TooltipTargetPolicy final {
    using Model=NativeModel;using Controller=NativeController;using Presentation=NativePresentation;using Scheduler=NativeScheduler;
    using Style=TooltipPresentationStyleValue<Motion>;using Input=TooltipInputValue<Motion>;using Options=TooltipTargetOptions;
    using Diagnostic=TooltipDiagnostic;using Configuration=TooltipConfiguration;using GeometryInput=TooltipGeometryInput;
    using Interaction=TooltipInteraction;using Phase=TooltipPhase;using Point=TooltipPoint;using Rect=TooltipRect;
};
template<template<class>class Host,class Motion,class Model,class Controller,class Presentation,class Scheduler>
using TooltipTargetBinding=Host<TooltipTargetPolicy<Motion,Model,Controller,Presentation,Scheduler>>;
} // namespace lvui::native
