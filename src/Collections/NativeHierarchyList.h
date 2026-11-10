#pragma once
#include <LVUI/NativeIconCache.h>
#include <LVUI/HierarchyDrag.h>
#include <LVUI/HierarchyItemProjection.h>
#include <LVUI/HierarchyRowContext.h>
#include <LVUI/HierarchyViewport.h>
#include <LVUI/NativeHierarchyItem.h>
#include <LVUI/NativeInputEvents.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
namespace lvui::native {
struct HierarchyListInput final {
    std::shared_ptr<HierarchyController> controller;
    std::shared_ptr<HierarchyEditor> editor;
    HierarchyItemConfiguration row_appearance;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t source_revision = 0, view_revision = 0, icon_revision = 0;
    double width = 200, height = 400, row_height = 20;
    std::size_t overscan = 1;
    bool enabled = true, show_indicator = true, keyboard_navigation = true, editable = false;
    friend bool operator==(const HierarchyListInput&, const HierarchyListInput&) = default;
};
struct HierarchyListEvent final {
    std::string key;
    std::optional<std::int64_t> id;
    std::size_t source_index = 0;
};
struct NativeHierarchyListCallbacks final {
    std::function<void(const HierarchyListEvent&)> activated;
    std::function<void(std::string, bool)> expansion_changed;
    std::function<void(const HierarchyMovePlan&)> moved;
};
template<class Builder>using HierarchyCountViewFactoryValue=std::function<ii::core::Result<void>(Builder&,std::shared_ptr<const HierarchyRowContext>)>;
template<class Builder>using HierarchyRowViewFactoryValue=HierarchyCountViewFactoryValue<Builder>;
template<class NativeItemModel,class NativeItem,class Image,class Decoder,class Builder,class NativeViewport,class NativeDrag>
struct HierarchyListHostPolicy final {
    using Input=HierarchyListInput;using ItemInput=HierarchyItemInputValue<Image>;using ItemModel=NativeItemModel;using Item=NativeItem;
    using IconSource=lvui::IconSource;using IconCache=NativeIconCache<Decoder>;
    using ViewportSource=NativeViewport;using ViewportAnchor=HierarchyViewportAnchor;using ViewportRequest=HierarchyViewportRequest;
    using Projection=HierarchyProjection;using VisibleEntry=HierarchyVisibleEntry;using RowContext=HierarchyRowContext;using RowSnapshot=HierarchyRowSnapshot;
    using Drag=NativeDrag;using DragPoint=HierarchyDragPoint;using Error=HierarchyError;using ErrorCode=HierarchyErrorCode;
    using Callbacks=NativeHierarchyListCallbacks;using ViewFactory=HierarchyCountViewFactoryValue<Builder>;
    [[nodiscard]] static auto project(const VisibleEntry& value,HierarchyItemConfiguration appearance){return hierarchy_item_configuration<ItemModel>(value,std::move(appearance));}
    [[nodiscard]] static std::shared_ptr<RowContext> make_context(RowSnapshot value){return HierarchyRowContextPort::make(std::move(value));}
    static void publish_context(RowContext& context,RowSnapshot value){HierarchyRowContextPort::publish(context,std::move(value));}
    static void close_context(RowContext& context)noexcept{HierarchyRowContextPort::close(context);}
    template<class Kind>[[nodiscard]] static bool pointer_canceled(Kind value)noexcept{return lvui::native::pointer_canceled(value);}
};
template<template<class>class Host,class ItemModel,class Item,class Image,class Decoder,class Builder,class Viewport,class Drag>
using HierarchyListBinding=Host<HierarchyListHostPolicy<ItemModel,Item,Image,Decoder,Builder,Viewport,Drag>>;
} // namespace lvui::native
