#pragma once

#include <LVUI/ListItem.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace lvui {

enum class ListItemPreviewVariant { resource,media };
struct ListItemPreviewRowConfiguration final {
    ListItemPreviewVariant variant=ListItemPreviewVariant::resource;
    double width=0,height=0,horizontal_padding=12,vertical_padding=12,content_spacing=8,section_spacing=12;
    double preview_size=48,action_width=72,segment_width=122,segment_height=29;
    std::optional<double> preview_width,preview_height;
    bool show_preview=true,show_description=true,show_metadata=true,show_more=true,show_segments=true;
    bool show_primary_action=true,show_secondary_action=true;
    friend bool operator==(const ListItemPreviewRowConfiguration&,const ListItemPreviewRowConfiguration&)=default;
};
struct ListItemPreviewRowGeometry final {
    ListItemRect preview,label,description,more,duration,media_detail,segments,status,actions;
    std::array<ListItemRect,3> metadata{};
};

// Resource and media items share a preview but keep their metadata and actions distinct.

struct ListItemPreviewRowPolicy final {
    using ListItemPreviewRowConfiguration=::lvui::ListItemPreviewRowConfiguration;
    using ListItemPreviewRowGeometry=::lvui::ListItemPreviewRowGeometry;
    using ListItemSize=::lvui::ListItemSize;
    using ListItemRect=::lvui::ListItemRect;
    using ListItemPreviewVariant=::lvui::ListItemPreviewVariant;
};
template<template<class>class Engine>using ListItemPreviewRowBinding=Engine<ListItemPreviewRowPolicy>;

} // namespace lvui
