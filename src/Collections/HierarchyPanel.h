#pragma once

#include <LVUI/StyleTokens.h>
#include <LVUI/PanelRegions.h>

#include <algorithm>
#include <cmath>
#include <optional>

namespace lvui {

struct HierarchyPanelConfiguration final {
    double width = 200, height = 530, toolbar_height = 26, footer_height = 26;
    bool toolbar_visible = true, footer_visible = false, enabled = true;
    StyleColor background{.0951643139,.0965511352,.0979379490,1};
    friend bool operator==(const HierarchyPanelConfiguration&, const HierarchyPanelConfiguration&) = default;
};
using HierarchyPanelRect=PanelRegionRect;
using HierarchyPanelGeometry=PanelRegions;

[[nodiscard]] inline bool valid_hierarchy_panel_configuration(const HierarchyPanelConfiguration& value) noexcept {
    const auto bounded = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
    const auto c = value.background;
    return bounded(value.width,1,8192) && bounded(value.height,1,8192) &&
        bounded(value.toolbar_height,0,1024) && bounded(value.footer_height,0,1024) &&
        bounded(c.red,0,1) && bounded(c.green,0,1) && bounded(c.blue,0,1) && bounded(c.alpha,0,1);
}

// Toolbar and footer retain their requested heights where space permits. A
// smaller allocation clips them in order and never creates negative regions.
[[nodiscard]] inline std::optional<HierarchyPanelGeometry> hierarchy_panel_geometry(
        const HierarchyPanelConfiguration& value, double width, double height) noexcept {
    if (!valid_hierarchy_panel_configuration(value)) return std::nullopt;
    return panel_regions(width,height,value.toolbar_height,value.footer_height,value.toolbar_visible,value.footer_visible);
}

} // namespace lvui
