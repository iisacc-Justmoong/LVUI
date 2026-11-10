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

template<class ConfigurationType>struct PanelLayoutPolicy final {
    using Configuration=ConfigurationType;using Rect=PanelRegionRect;using Regions=PanelRegions;
};
template<template<class>class Model,class Configuration>
using PanelLayoutBinding=Model<PanelLayoutPolicy<Configuration>>;

} // namespace lvui
