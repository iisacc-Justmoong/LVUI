#pragma once

#include <LVUI/Hierarchy.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lvui {

enum class HierarchyReveal { contain, start, end, center };
struct HierarchyViewportAnchor final {
    std::string key;
    std::size_t index = 0;
    double within = 0;
    std::uint64_t movement = 0;
};
struct HierarchyViewportRequest final {
    double width = 0, height = 0, offset = 0, row_height = 20;
    std::size_t overscan = 1;
    std::uint64_t movement = 0;
    HierarchyViewportAnchor anchor;
    std::string reveal_key;
    HierarchyReveal reveal = HierarchyReveal::contain;
};
struct HierarchyViewportRow final { std::size_t index = 0; double y = 0, height = 0; };
template<class Source>
struct HierarchyViewportFrameValue final {
    std::shared_ptr<const Source> source;
    std::vector<HierarchyViewportRow> rows;
    double width = 0, height = 0, content_height = 0, offset = 0;
    std::uint64_t source_revision = 0, view_revision = 0, movement = 0;
    HierarchyViewportAnchor anchor;
    std::string revealed_key;
};

// Index one immutable visible projection. Scrolling reuses its key map and
// materializes only the viewport plus bounded overscan, not the whole tree.
struct HierarchyViewportPolicy final {
    using HierarchyProjection=::lvui::HierarchyProjection;
    using HierarchyViewportRequest=::lvui::HierarchyViewportRequest;
    using HierarchyReveal=::lvui::HierarchyReveal;
    using HierarchyError=::lvui::HierarchyError;
    using HierarchyErrorCode=::lvui::HierarchyErrorCode;
    template<class Source>using Frame=HierarchyViewportFrameValue<Source>;
    static constexpr std::size_t maximum_items=hierarchy_maximum_items;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<template<class>class Engine>using HierarchyViewportBinding=Engine<HierarchyViewportPolicy>;

} // namespace lvui
