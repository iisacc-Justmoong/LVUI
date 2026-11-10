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
    StyleColor background{figma_defaults::panel_sub_surface};
    friend bool operator==(const HierarchyPanelConfiguration&, const HierarchyPanelConfiguration&) = default;
};
using HierarchyPanelRect=PanelRegionRect;
using HierarchyPanelGeometry=PanelRegions;


} // namespace lvui
