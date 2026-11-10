#pragma once

#include <LVUI/Button.h>
#include <LVUI/ColorSource.h>
#include <LVUI/Text.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace lvui {

enum class AlertShape { rounded, cylinder };

struct AlertPalette final {
    StyleColor frame{figma_defaults::with_alpha(figma_defaults::panel_surface_panel, .85)};
    StyleColor backdrop{0, 0, 0, .4};
    StyleColor border{1, 1, 1, .2};
    StyleColor title{1, 1, 1, .9};
    StyleColor description{1, 1, 1, .8};
    friend bool operator==(const AlertPalette&, const AlertPalette&) = default;
};

struct AlertLayoutMetrics final {
    double action_horizontal_inset=24,action_gap=14,action_height=56,tertiary_action_height=44;
    double three_top_spacing=18,three_bottom_spacing=18,action_vertical_gap=12,two_top_spacing=28,bottom_spacing=32;
    double rounded_radius_fraction=.4;
    friend bool operator==(const AlertLayoutMetrics&,const AlertLayoutMetrics&)=default;
};

struct AlertConfiguration final {
    std::string title, description, message, accessibility_name = "Alert";
    std::array<std::string, 3> action_text;
    std::array<bool, 3> action_enabled{true, true, true};
    unsigned action_count = 0; // Zero selects a count from the supplied labels.
    std::optional<bool> secondary_destructive;
    std::optional<StyleColor> primary_color;
    bool show_icon = true, dismiss_on_background = false, dismiss_on_escape = false;
    bool motion_enabled = true, glass_enabled = false, shadow_enabled = false;
    double minimum_width = 280, maximum_width = 500, preferred_width = 500, side_padding = 24;
    double content_top = 46, content_bottom = 36, text_padding = 24, icon_spacing = 28, text_spacing = 14;
    double icon_extent = 86, corner_radius = 36, border_width = 1, blur_radius = 28;
    double enter_duration_ms = 420, exit_duration_ms = 150, overshoot = 1.45, initial_scale = .92;
    AlertShape shape = AlertShape::rounded;
    AlertPalette palette;
    AlertLayoutMetrics metrics;
    std::string fallback_action="Button";
    friend bool operator==(const AlertConfiguration&, const AlertConfiguration&) = default;
};

struct AlertRect final {
    double x = 0, y = 0, width = 0, height = 0;
    friend bool operator==(const AlertRect&, const AlertRect&) = default;
};

struct AlertAction final {
    std::string_view text;
    bool visible = false, enabled = false, destructive = false;
    ButtonTone tone = ButtonTone::normal;
};

// Child rectangles are local to the centered frame. Text heights are measured
// by the host using the width and fonts selected by the application adapter.
struct AlertLayout final {
    AlertRect frame, icon, title, description;
    std::array<AlertRect, 3> actions;
    double radius = 0;
};

} // namespace lvui
