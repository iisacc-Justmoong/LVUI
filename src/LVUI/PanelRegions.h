#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

namespace lvui {

struct PanelRegionRect final {
    double x=0,y=0,width=0,height=0;
    friend bool operator==(const PanelRegionRect&,const PanelRegionRect&)=default;
};
struct PanelRegions final {
    PanelRegionRect toolbar,list,footer;
    bool toolbar_visible=false,list_visible=false,footer_visible=false;
    friend bool operator==(const PanelRegions&,const PanelRegions&)=default;
};

// Layout is independent of the concrete toolbar, list and footer ports.
[[nodiscard]] inline std::optional<PanelRegions> panel_regions(double width,double height,double toolbar_height,
        double footer_height,bool toolbar_visible,bool footer_visible)noexcept{
    const auto bound=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
    if(!bound(width,0,8192)||!bound(height,0,8192)||!bound(toolbar_height,0,1024)||!bound(footer_height,0,1024))return std::nullopt;
    const auto toolbar=toolbar_visible?std::min(toolbar_height,height):0.;
    const auto footer=footer_visible?std::min(footer_height,height-toolbar):0.;
    const auto list=height-toolbar-footer;
    return PanelRegions{.toolbar={0,0,width,toolbar},.list={0,toolbar,width,list},.footer={0,toolbar+list,width,footer},
        .toolbar_visible=width>0&&toolbar>0,.list_visible=width>0&&list>0,.footer_visible=width>0&&footer>0};
}

} // namespace lvui
