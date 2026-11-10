#pragma once
#include <LVUI/MobileNavigationTokens.generated.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace lvui {
struct MobileIndicatorGeometry final {
    double left=0,right=0,height=mobile_navigation_tokens::touch_size;
    friend bool operator==(const MobileIndicatorGeometry&,const MobileIndicatorGeometry&)=default;
};
// A frame-time driven value model. It never owns a timer or host object.
struct MobileNavigationMotionPolicy final {
 using Geometry=MobileIndicatorGeometry;inline static constexpr auto touch_size=mobile_navigation_tokens::touch_size;
 inline static constexpr double stretch_duration=140,stretch_fraction=.75,stretch_amount=6,leading_forward=.15,trailing_forward=.88,overshoot=1.1;
};
template<template<class>class Motion>using MobileNavigationMotionBinding=Motion<MobileNavigationMotionPolicy>;

} // namespace lvui
