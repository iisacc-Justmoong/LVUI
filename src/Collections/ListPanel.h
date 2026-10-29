#pragma once

#include <LVUI/PanelRegions.h>
#include <LVUI/StyleTokens.h>

#include <cmath>
#include <optional>

namespace lvui {

struct ListPanelConfiguration final {
    double width=170,height=140,toolbar_height=30,footer_height=26;
    bool toolbar_visible=false,footer_visible=true,enabled=true,interactive=true;
    StyleColor background{.0951643139,.0965511352,.0979379490,1};
    friend bool operator==(const ListPanelConfiguration&,const ListPanelConfiguration&)=default;
};
[[nodiscard]] inline bool valid_list_panel_configuration(const ListPanelConfiguration& value)noexcept{
    const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};const auto c=value.background;
    return range(value.width,1,8192)&&range(value.height,1,8192)&&range(value.toolbar_height,0,1024)&&range(value.footer_height,0,1024)&&
        range(c.red,0,1)&&range(c.green,0,1)&&range(c.blue,0,1)&&range(c.alpha,0,1);
}
[[nodiscard]] inline std::optional<PanelRegions> list_panel_geometry(const ListPanelConfiguration& value,double width,double height)noexcept{
    if(!valid_list_panel_configuration(value))return std::nullopt;
    return panel_regions(width,height,value.toolbar_height,value.footer_height,value.toolbar_visible,value.footer_visible);
}

} // namespace lvui
