#pragma once
#include <LVUI/ListViewValues.h>
#include <LVUI/ListViewport.h>
#include <LVUI/ListDrag.h>
#include <LVUI/ListEditing.h>
#include <LVUI/ListRowContext.h>
#include <LVUI/NativeInputField.h>
#include <LVUI/NativeInputEvents.h>
#include <LVUI/NativeListItemDrawing.h>
#include <LVUI/NativeIconCache.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
namespace lvui::native {
template<class NativeDrawing,class NativeField,class Image,class Decoder,class Builder,class NativeViewport,class NativeDrag>
struct ListViewHostPolicy final {
    using Input=ListViewInput;using Callbacks=NativeListViewCallbacks;using ViewFactory=ListRowViewFactoryValue<Builder>;
    using Drawing=NativeDrawing;using ItemConfiguration=ListItemConfiguration;using InputField=NativeField;using InputFieldConfiguration=lvui::InputFieldConfiguration;
    using IconCache=NativeIconCache<Decoder>;using Color=StyleColor;using Snapshot=ListSnapshot;using Entry=ListEntry;using Controller=ListController;
    using ViewportSource=NativeViewport;using ViewportAnchor=ListViewportAnchor;using Reveal=ListReveal;
    using Error=ListError;using ErrorCode=ListErrorCode;using EditEvent=ListEditEvent;using ViewEvent=ListViewEvent;
    using Editing=ListEditing;using Drag=NativeDrag;using DragPoint=ListDragPoint;using DragProposal=ListDragProposal;
    using RowContext=ListRowContext;using RowSnapshot=ListRowSnapshot;static constexpr std::size_t max_items=list_maximum_items;
    [[nodiscard]] static std::string display_text(std::string_view value){return native_display_text(value);}
    template<class Kind>[[nodiscard]] static bool pointer_canceled(Kind value)noexcept{return lvui::native::pointer_canceled(value);}
    [[nodiscard]] static auto make_action(std::function<ii::core::Result<void>()> callback){return ListRowContextPort::make_action(std::move(callback));}
    [[nodiscard]] static auto make_context(RowSnapshot value){return ListRowContextPort::make(std::move(value));}
    static void publish_context(RowContext& context,RowSnapshot value){ListRowContextPort::publish(context,std::move(value));}
    static void close_context(RowContext& context)noexcept{ListRowContextPort::close(context);}
};
template<template<class>class Host,class Drawing,class Field,class Image,class Decoder,class Builder,class Viewport,class Drag>
using ListViewBinding=Host<ListViewHostPolicy<Drawing,Field,Image,Decoder,Builder,Viewport,Drag>>;
} // namespace lvui::native
