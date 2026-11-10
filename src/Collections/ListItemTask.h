#pragma once

#include <LVUI/ListItem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace lvui {

// Layout data is independent of the native host and the task's application model.
struct ListItemTaskConfiguration final {
    double width=0,height=0,horizontal_padding=12,vertical_padding=12,content_spacing=8,section_spacing=12;
    double selection_size=18,selector_width=97,quantity_width=48,quantity_spacing=4,stepper_size=18,action_width=72;
    bool show_selection=true,show_selector=true,show_description=true,show_metadata=true,show_stepper=true;
    bool show_primary_action=true,show_secondary_action=true;
    friend bool operator==(const ListItemTaskConfiguration&,const ListItemTaskConfiguration&)=default;
};
struct ListItemTaskGeometry final {
    ListItemRect selection,label,selector,description,status,quantity_label,quantity_stepper,actions;
    std::array<ListItemRect,3> metadata{};
};


struct ListItemTaskPolicy final {
    using ListItemTaskConfiguration=::lvui::ListItemTaskConfiguration;
    using ListItemTaskGeometry=::lvui::ListItemTaskGeometry;
    using ListItemSize=::lvui::ListItemSize;
    using ListItemRect=::lvui::ListItemRect;
};
template<template<class>class Engine>using ListItemTaskBinding=Engine<ListItemTaskPolicy>;

} // namespace lvui
