#pragma once

#include <LVUI/Button.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace lvui {

using ToolbarButtonId = std::variant<std::int64_t, std::string>;
struct ToolbarButtonPalette final {
    StyleColor text{1, 1, 1, .8}, disabled_text{1, 1, 1, .3};
    StyleColor hover{figma_defaults::panel_surface_alt};
    StyleColor pressed{figma_defaults::accent_blue_subtle};
    StyleColor primary{figma_defaults::accent_blue_default};
    StyleColor danger{figma_defaults::accent_red_default};
    StyleColor focus{figma_defaults::accent_blue_default};
    friend bool operator==(const ToolbarButtonPalette&, const ToolbarButtonPalette&) = default;
};
struct ToolbarButtonConfiguration final {
    ToolbarButtonId id{std::int64_t(-1)};
    std::string text, accessibility_name;
    ButtonTone tone = ButtonTone::borderless;
    bool enabled = true, visible = true, has_icon = false;
    double width = 0, height = 22, icon_size = 18, left_padding = 2, right_padding = 2;
    double spacing = 4, corner_radius = 8, font_size = 12;
    ToolbarButtonPalette palette;
    friend bool operator==(const ToolbarButtonConfiguration&, const ToolbarButtonConfiguration&) = default;
};
struct ToolbarButtonRect final { double x = 0, y = 0, width = 0, height = 0; };
struct ToolbarButtonGeometry final { ToolbarButtonRect icon, text; double radius = 0; };
struct ToolbarButtonVisual final { StyleColor background, text, focus; bool enabled = false, focused = false; };

} // namespace lvui
