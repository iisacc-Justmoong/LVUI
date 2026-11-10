#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace lvui {

struct ListItemQuantityConfiguration final {
    double value=1,minimum=0,maximum=std::numeric_limits<double>::infinity(),step=1;
    bool enabled=true;
    friend bool operator==(const ListItemQuantityConfiguration&,const ListItemQuantityConfiguration&)=default;
};

// State and range policy are independent of the stepper input control.

struct ListItemQuantityPolicy final {
    using ListItemQuantityConfiguration=::lvui::ListItemQuantityConfiguration;
};
template<template<class>class Engine>using ListItemQuantityBinding=Engine<ListItemQuantityPolicy>;

} // namespace lvui
