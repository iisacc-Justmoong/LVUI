#pragma once

#include <LVUI/ListItem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace lvui {

// Geometry for the multi-section detail row is independent of its renderer.
// Zero width/height requests the natural extent, as in the other list models.
struct ListItemDetailActionsConfiguration final {
    double width=0,height=0,horizontal_padding=12,vertical_padding=12;
    double content_spacing=8,section_spacing=12,icon_size=18,action_width=72;
    std::optional<double> leading_width,leading_height;
    bool show_leading_icon=true,show_bookmark=true,show_description=true,show_metadata=true;
    std::array<bool,3> show_actions{true,true,true};
    friend bool operator==(const ListItemDetailActionsConfiguration&,const ListItemDetailActionsConfiguration&)=default;
};
struct ListItemDetailActionsGeometry final {
    ListItemRect leading,label,description,bookmark,status,actions;
    std::array<ListItemRect,3> metadata{};
};


struct ListItemDetailActionsPolicy final {
    using ListItemDetailActionsConfiguration=::lvui::ListItemDetailActionsConfiguration;
    using ListItemDetailActionsGeometry=::lvui::ListItemDetailActionsGeometry;
    using ListItemSize=::lvui::ListItemSize;
    using ListItemRect=::lvui::ListItemRect;
};
template<template<class>class Engine>using ListItemDetailActionsBinding=Engine<ListItemDetailActionsPolicy>;

} // namespace lvui
