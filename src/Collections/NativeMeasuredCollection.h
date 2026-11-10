#pragma once
#include <LVUI/NativeVirtualCollection.h>

namespace lvui::native {
struct VirtualRowMeasurement final {std::string key;double width=0,height=0;};
template<class T,class Planner> struct VirtualMeasurementsPolicy final {
    using Item=T;
    using Source=VirtualCollectionSource<T>;
    using Layout=VirtualCollectionLayout;
    using Viewport=VirtualViewportState;
    using Anchor=VirtualAnchor;
    using Measurement=VirtualRowMeasurement;
    using PlannerType=Planner;
};
template<template<class>class Engine,class T,class Planner>
using VirtualMeasurementsBinding=Engine<VirtualMeasurementsPolicy<T,Planner>>;
} // namespace lvui::native
