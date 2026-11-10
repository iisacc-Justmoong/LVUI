#pragma once

#include <LVUI/Hierarchy.h>
#include <LVUI/HierarchyViewport.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace lvui {

struct HierarchyDragPoint final { double x = 0, y = 0; };
template<class Source>
struct HierarchyDragViewportValue final {
    std::shared_ptr<const Source> source;
    double width = 0, height = 0, offset = 0, row_height = 20;
};
struct HierarchyDragProposal final {
    HierarchyMove request;
    HierarchyMovePlan plan;
};
struct HierarchyDragUpdate final {
    bool dragging = false;
    int scroll_direction = 0;
    std::optional<HierarchyDragProposal> proposal;
};

// A drag previews an immutable snapshot; only an injected HierarchyEditor can
// commit the eventual revision-checked request. Pointer motion is view state.
template<class Source>struct HierarchyDragPolicy final {
    using HierarchySnapshot=::lvui::HierarchySnapshot;
    using HierarchyIndex=::lvui::HierarchyIndex;
    using HierarchyDragPoint=::lvui::HierarchyDragPoint;
    using HierarchyDragUpdate=::lvui::HierarchyDragUpdate;
    using HierarchyDragProposal=::lvui::HierarchyDragProposal;
    using HierarchyMove=::lvui::HierarchyMove;
    using HierarchyPlacement=::lvui::HierarchyPlacement;
    using HierarchyError=::lvui::HierarchyError;
    using HierarchyErrorCode=::lvui::HierarchyErrorCode;
    using Viewport=HierarchyDragViewportValue<Source>;
};
template<template<class>class Engine,class Source>using HierarchyDragBinding=Engine<HierarchyDragPolicy<Source>>;

} // namespace lvui
