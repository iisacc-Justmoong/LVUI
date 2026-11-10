#pragma once
#include <LVUI/HierarchyPanel.h>
#include <LVUI/NativeHierarchyList.h>
#include <LVUI/NativeHierarchyToolbar.h>
#include <LVUI/NativeListFooter.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
namespace lvui::native {
template<class Color> [[nodiscard]] inline StyleColor hierarchy_panel_color(const Color& value) noexcept {
    return {value.red,value.green,value.blue,value.alpha};
}

struct HierarchyPanelInput final {
    HierarchyPanelConfiguration appearance;
    HierarchyToolbarInput toolbar;
    HierarchyListInput list;
    ListFooterInput footer;
    friend bool operator==(const HierarchyPanelInput&, const HierarchyPanelInput&) = default;
};
struct NativeHierarchyPanelCallbacks final {
    std::function<void(const HierarchyListEvent&)> item_activated;
    std::function<void(std::string,bool)> expansion_changed;
    std::function<void(const HierarchyMovePlan&)> item_moved;
    std::function<void(const HierarchyToolbarSelection&)> toolbar_active_changed;
    std::function<void(const HierarchyToolbarSelection&)> toolbar_triggered;
    std::function<void(std::size_t,const ListFooterSlot&)> footer_clicked;
};

template<class NativeLayout,class NativeToolbarLayout,class NativeFooterModel,class NativeList,class NativeToolbar,class NativeFooter,class Builder>
struct HierarchyPanelHostPolicy final {
    using Input=HierarchyPanelInput;using ListInput=HierarchyListInput;using ToolbarInput=HierarchyToolbarInput;using FooterInput=ListFooterInput;
    using Geometry=HierarchyPanelGeometry;using Rect=HierarchyPanelRect;using Color=StyleColor;using Callbacks=NativeHierarchyPanelCallbacks;
    using Layout=NativeLayout;using ToolbarLayout=NativeToolbarLayout;using FooterModel=NativeFooterModel;
    using List=NativeList;using Toolbar=NativeToolbar;using Footer=NativeFooter;using Controller=HierarchyController;
    using ViewFactory=HierarchyCountViewFactoryValue<Builder>;
    [[nodiscard]] static std::shared_ptr<Controller> make_controller(){return std::make_shared<Controller>(std::make_shared<HierarchyMemory>());}
};
template<template<class>class Host,class Layout,class ToolbarLayout,class FooterModel,class List,class Toolbar,class Footer,class Builder>
using HierarchyPanelBinding=Host<HierarchyPanelHostPolicy<Layout,ToolbarLayout,FooterModel,List,Toolbar,Footer,Builder>>;
} // namespace lvui::native
