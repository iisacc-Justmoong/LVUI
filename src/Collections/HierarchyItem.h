#pragma once

#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace lvui {

struct HierarchyItemPalette final {
    StyleColor idle{0, 0, 0, 0}, hover{1, 1, 1, .06}, pressed{1, 1, 1, .12};
    StyleColor active{figma_defaults::with_alpha(figma_defaults::accent_blue_default, .24)}, inactive{1, 1, 1, .12}, drag{figma_defaults::with_alpha(figma_defaults::accent_blue_default, .16)};
    StyleColor text{1, 1, 1, .9}, disabled_text{1, 1, 1, .35}, focus{figma_defaults::accent_blue_default};
    friend bool operator==(const HierarchyItemPalette&, const HierarchyItemPalette&) = default;
};
struct HierarchyItemConfiguration final {
    std::string key, label, icon_name, accessibility_name;
    std::optional<std::int64_t> id;
    std::int64_t count = -1;
    std::size_t depth = 0;
    double width = 200, height = 20, indent_step = 8, icon_size = 18, chevron_size = 16;
    double left_padding = 8, right_padding = 8, leading_spacing = 2, count_width = 24, corner_radius = 5;
    bool enabled = true, activatable = true, selected = false, expanded = false, show_chevron = true;
    bool has_children = false, drag_preview = false;
    HierarchyItemPalette palette;
    friend bool operator==(const HierarchyItemConfiguration&, const HierarchyItemConfiguration&) = default;
};
struct HierarchyItemRect final {
    double x = 0, y = 0, width = 0, height = 0;
    friend bool operator==(const HierarchyItemRect&, const HierarchyItemRect&) = default;
};
struct HierarchyItemGeometry final {
    HierarchyItemRect surface, icon, label, count, chevron;
    bool show_icon = false, show_count = false, show_chevron = false;
};
enum class HierarchyItemPhase { idle, hover, pressed, active, inactive, drag };
struct HierarchyItemInteraction final { bool hovered = false, pressed = false, focused = false; };
struct HierarchyItemVisual final {
    HierarchyItemPhase phase = HierarchyItemPhase::idle;
    StyleColor background, text, focus;
    bool focused = false;
};

// LVUI owns row metrics, state precedence and expansion policy; the app owns
// structural edits and can replace the backing tree independently.
struct HierarchyItemModelPolicy final {
 using Configuration=HierarchyItemConfiguration;using Geometry=HierarchyItemGeometry;using Interaction=HierarchyItemInteraction;
 using Visual=HierarchyItemVisual;using Phase=HierarchyItemPhase;using Color=StyleColor;
 inline static constexpr double count_gap=8;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<template<class>class Model>using HierarchyItemModelBinding=Model<HierarchyItemModelPolicy>;

} // namespace lvui
