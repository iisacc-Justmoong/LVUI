#pragma once
#include <LVUI/Card.h>
#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeVisual.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor card_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
[[nodiscard]] inline CardType card_type(CardType value)noexcept{return value;}
[[nodiscard]] inline CardSize card_size(CardSize value)noexcept{return value;}
[[nodiscard]] inline CardDetail card_detail(CardDetail value)noexcept{return value;}
[[nodiscard]] inline CardDisplayState card_display_state(CardDisplayState value)noexcept{return value;}
template<class Image>struct CardInputValue final {CardConfiguration configuration;std::shared_ptr<const Image> icon,preview;
 std::shared_ptr<const IconSource> icon_source;std::int64_t icon_revision=0;friend bool operator==(const CardInputValue&,const CardInputValue&)=default;};
struct NativeCardCallbacks final {std::function<void()> clicked;std::function<void(bool)> selected_changed;std::function<void()> menu_requested,action_triggered,double_clicked;};
template<class Factory>struct NativeCardViewsValue final {Factory content,preview,icon;};
template<class NativeModel,class Decoder,class Factory>struct CardHostPolicy final {
 using Model=NativeModel;using Input=CardInputValue<typename Decoder::Image>;using Callbacks=NativeCardCallbacks;using Views=NativeCardViewsValue<Factory>;
 using Type=CardType;using Rect=CardRect;using Color=StyleColor;using IconCache=NativeIconCache<Decoder>;
 [[nodiscard]] static std::string display_text(std::string_view value){return native_display_text(value);}
};
template<template<class>class NativeModel,template<class>class NativeHost,class Decoder,class Factory>
using CardBinding=NativeHost<CardHostPolicy<NativeModel<CardModelPolicy>,Decoder,Factory>>;
} // namespace lvui::native
