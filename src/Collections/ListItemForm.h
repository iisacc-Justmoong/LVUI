#pragma once

#include <LVUI/ListItem.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace lvui {

struct ListItemFormConfiguration final {
    double width=0,height=0,horizontal_padding=12,vertical_padding=12,content_spacing=8,section_spacing=12;
    double selector_width=97,input_width=140,input_height=30,action_width=72;
    bool show_description=true,show_selector=true,show_input1=true,show_input2=true,show_toggle=true;
    bool show_primary_action=true,show_secondary_action=true;
    friend bool operator==(const ListItemFormConfiguration&,const ListItemFormConfiguration&)=default;
};
struct ListItemFormGeometry final {
    ListItemRect label,description,selector,field_label1,input1,field_label2,input2,toggle,status,actions;
};


struct ListItemFormPolicy final {
    using ListItemFormConfiguration=::lvui::ListItemFormConfiguration;
    using ListItemFormGeometry=::lvui::ListItemFormGeometry;
    using ListItemSize=::lvui::ListItemSize;
    using ListItemRect=::lvui::ListItemRect;
};
template<template<class>class Engine>using ListItemFormBinding=Engine<ListItemFormPolicy>;

} // namespace lvui
