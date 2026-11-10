#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace lvui {
enum class TooltipPlacement { automatic, above, below, left, right };
struct TooltipPoint final {
    double x=0,y=0;
    friend bool operator==(const TooltipPoint&,const TooltipPoint&)=default;
};
struct TooltipSize final {
    double width=0,height=0;
    friend bool operator==(const TooltipSize&,const TooltipSize&)=default;
};
struct TooltipRect final {
    double x=0,y=0,width=0,height=0;
    friend bool operator==(const TooltipRect&,const TooltipRect&)=default;
};
struct TooltipConfiguration final {
    TooltipPlacement preferred=TooltipPlacement::automatic;
    double maximum_width=320,maximum_height=320,content_padding=12,edge_margin=8;
    double corner_radius=12,tail_length=10,tail_width=16;
    bool automatic=true;
    std::chrono::milliseconds delay{500},timeout{5000},hide_delay{120},animation_duration{420};
    friend bool operator==(const TooltipConfiguration&,const TooltipConfiguration&)=default;
};
struct TooltipLayout final {
    TooltipPlacement placement=TooltipPlacement::above;
    TooltipRect body,frame;
    TooltipPoint tip,tail_start,tail_end;
    double radius=0;
    friend bool operator==(const TooltipLayout&,const TooltipLayout&)=default;
};
struct TooltipInteraction final {
    bool target_present=true,target_available=false,target_engaged=false,bubble_hovered=false;
};
enum class TooltipPhase { hidden,waiting_to_open,shown,waiting_to_hide };
struct TooltipTransition final {bool opened=false,closed=false;};

struct TooltipModelPolicy final {
    using Configuration=TooltipConfiguration;using Interaction=TooltipInteraction;using Placement=TooltipPlacement;using Phase=TooltipPhase;
    using Transition=TooltipTransition;using Rect=TooltipRect;using Point=TooltipPoint;using Size=TooltipSize;using Layout=TooltipLayout;
};
template<template<class>class Model>using TooltipModelBinding=Model<TooltipModelPolicy>;

} // namespace lvui
