#pragma once

#include <LVUI/ListItem.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace lvui {

enum class ListItemDetailControlVariant { quantity,settings };
struct ListItemDetailControlsConfiguration final {
    ListItemDetailControlVariant variant=ListItemDetailControlVariant::quantity;
    double width=0,height=0,horizontal_padding=12,vertical_padding=12,content_spacing=8,section_spacing=12;
    double icon_size=18,selector_width=97,unit_selector_width=97,quantity_width=48,quantity_spacing=4,action_width=72;
    std::optional<double> leading_width,leading_height;
    double segment_width=122,segment_height=29;
    bool show_description=true,show_leading_icon=true,show_toggle=true,show_selector=true,show_unit_selector=true;
    bool show_stepper=true,show_segments=true,show_primary_action=true,show_secondary_action=true;
    friend bool operator==(const ListItemDetailControlsConfiguration&,const ListItemDetailControlsConfiguration&)=default;
};
struct ListItemDetailControlsGeometry final {
    ListItemRect leading,label,description,header_selector,toggle;
    ListItemRect quantity_caption,quantity_label,quantity_stepper,unit_selector;
    ListItemRect mode_caption,selector,scope_caption,segments,status,actions;
};

// These two compound rows share spacing and clipping rules while retaining
// separate controls and section heights. The host owns the actual widgets.

struct ListItemDetailControlsPolicy final {
    using ListItemDetailControlsConfiguration=::lvui::ListItemDetailControlsConfiguration;
    using ListItemDetailControlsGeometry=::lvui::ListItemDetailControlsGeometry;
    using ListItemSize=::lvui::ListItemSize;
    using ListItemRect=::lvui::ListItemRect;
    using ListItemDetailControlVariant=::lvui::ListItemDetailControlVariant;
};
template<template<class>class Engine>using ListItemDetailControlsBinding=Engine<ListItemDetailControlsPolicy>;

} // namespace lvui
