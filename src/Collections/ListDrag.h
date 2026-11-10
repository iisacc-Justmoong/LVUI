#pragma once

#include <LVUI/ListViewport.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace lvui {

struct ListDragPoint final {double x=0,y=0;};
template<class Source>
struct ListDragViewportValue final {
    std::shared_ptr<const Source> source;
    double width=0,height=0,offset=0,row_height=20;
};
struct ListDragProposal final {
    ListEdit request;
    std::size_t from=0,to=0;
    std::string target;
    bool after=false;
};
struct ListDragUpdate final {
    bool dragging=false;
    int scroll_direction=0;
    std::optional<ListDragProposal> proposal;
};

// Pointer motion only plans against a snapshot. An application-supplied editor
// must apply the resulting revision-checked move at release.
template<class Source>struct ListDragPolicy final {
    using ListSnapshot=::lvui::ListSnapshot;
    using ListIndex=::lvui::ListIndex;
    using ListDragPoint=::lvui::ListDragPoint;
    using ListDragProposal=::lvui::ListDragProposal;
    using ListDragUpdate=::lvui::ListDragUpdate;
    using ListEdit=::lvui::ListEdit;
    using ListEditKind=::lvui::ListEditKind;
    using ListError=::lvui::ListError;
    using ListErrorCode=::lvui::ListErrorCode;
    using Viewport=ListDragViewportValue<Source>;
};
template<template<class>class Engine,class Source>using ListDragBinding=Engine<ListDragPolicy<Source>>;

} // namespace lvui
