#pragma once

#include <LVUI/PanelRegions.h>
#include <LVUI/StyleTokens.h>

#include <cmath>
#include <optional>

namespace lvui {

struct ListPanelConfiguration final {
    double width=170,height=140,toolbar_height=30,footer_height=26;
    bool toolbar_visible=false,footer_visible=true,enabled=true,interactive=true;
    StyleColor background{figma_defaults::panel_sub_surface};
    friend bool operator==(const ListPanelConfiguration&,const ListPanelConfiguration&)=default;
};

} // namespace lvui
