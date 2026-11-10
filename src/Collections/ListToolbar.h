#pragma once

#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>

namespace lvui {

struct ListToolbarPalette final {
    StyleColor background{figma_defaults::panel_sub_surface};
    StyleColor hover{figma_defaults::panel_surface_alt};
    StyleColor pressed{figma_defaults::panel_surface_alt};
    StyleColor focus{figma_defaults::accent_blue_default};
    friend bool operator==(const ListToolbarPalette&, const ListToolbarPalette&) = default;
};
struct ListToolbarConfiguration final {
    std::array<std::string, 3> icons;
    double icon_size = 18, button_size = 22, horizontal_padding = 8, vertical_padding = 4, spacing = 4;
    double width = 0, height = 0, corner_radius = 8;
    bool enabled = true, interactive = true;
    ListToolbarPalette palette;
    friend bool operator==(const ListToolbarConfiguration&, const ListToolbarConfiguration&) = default;
};
struct ListToolbarRect final { double x = 0, y = 0, width = 0, height = 0; };
struct ListToolbarGeometry final {
    double width = 0, height = 0;
    std::array<ListToolbarRect, 3> slots, icons;
};

} // namespace lvui
