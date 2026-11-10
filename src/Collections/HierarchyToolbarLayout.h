#pragma once

#include <LVUI/HierarchyToolbar.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

namespace lvui {

struct HierarchyToolbarAppearance final {
    double minimum_width = 200, width = 0, height = 0, horizontal_padding = 8, vertical_padding = 2;
    double spacing = 0, slot_size = 22, icon_size = 18, corner_radius = 8, font_size = 12, background_opacity = 0;
    double slot_padding = 4, icon_text_spacing = 4;
    bool enabled = true, distribute_spacing = false;
    StyleColor background{figma_defaults::panel_sub_surface};
    ToolbarButtonPalette palette;
    friend bool operator==(const HierarchyToolbarAppearance&, const HierarchyToolbarAppearance&) = default;
};
struct HierarchyToolbarRect final { double x = 0, y = 0, width = 0, height = 0; };
struct HierarchyToolbarGeometry final { double width = 0, height = 0; std::vector<HierarchyToolbarRect> slots; };

} // namespace lvui
