#pragma once
#include <LVUI/ListPanel.h>
#include <LVUI/ListViewValues.h>
#include <LVUI/NativeListFooter.h>
#include <LVUI/NativeListToolbar.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
namespace lvui::native {
template<class Color> [[nodiscard]] inline StyleColor list_panel_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
struct ListPanelInput final {
    ListPanelConfiguration appearance;
    ListViewInput list;
    ListToolbarInput toolbar;
    ListFooterInput footer;
    friend bool operator==(const ListPanelInput&,const ListPanelInput&)=default;
};
struct NativeListPanelCallbacks final {
    std::function<void(const ListViewEvent&)> item_activated;
    std::function<void(const ListEditEvent&)> item_edited,item_submitted;
    std::function<void(const ListEditEvent&,const ListError&)> item_rejected;
    std::function<void(const ListDragProposal&)> item_moved;
    std::function<void(std::size_t,std::string)> toolbar_clicked;
    std::function<void(std::size_t,const ListFooterSlot&)> footer_clicked;
};

template<class NativeLayout,class NativeToolbarModel,class NativeFooterModel,class NativeList,class NativeToolbar,class NativeFooter,class NativeInputField,class Builder>
struct ListPanelHostPolicy final {
    using Input=ListPanelInput;using ListInput=ListViewInput;using ToolbarInput=ListToolbarInput;using FooterInput=ListFooterInput;
    using Geometry=PanelRegions;using Rect=PanelRegionRect;using Color=StyleColor;using Callbacks=NativeListPanelCallbacks;
    using Layout=NativeLayout;using ToolbarModel=NativeToolbarModel;using FooterModel=NativeFooterModel;
    using List=NativeList;using Toolbar=NativeToolbar;using Footer=NativeFooter;using InputField=NativeInputField;using Controller=ListController;
    using ViewFactory=ListRowViewFactoryValue<Builder>;
    [[nodiscard]] static ii::core::Result<std::shared_ptr<Controller>> make_controller(){
        auto memory=ListMemory::create();if(!memory)return ii::core::fail(ii::core::ErrorCode::invalid_argument,memory.error().message);
        auto controller=Controller::create(*memory);if(!controller)return ii::core::fail(ii::core::ErrorCode::invalid_argument,controller.error().message);
        return std::make_shared<Controller>(std::move(*controller));
    }
};
template<template<class>class Host,class Layout,class ToolbarModel,class FooterModel,class List,class Toolbar,class Footer,class InputField,class Builder>
using ListPanelBinding=Host<ListPanelHostPolicy<Layout,ToolbarModel,FooterModel,List,Toolbar,Footer,InputField,Builder>>;
} // namespace lvui::native
