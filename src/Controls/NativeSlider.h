#pragma once
#include <LVUI/NativeIconCache.h>
#include <LVUI/Slider.h>
namespace lvui::native {
template<class Color>inline StyleColor slider_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
template<class NativeModel,class Decoder>struct SliderHostPolicy final {
 using Model=NativeModel;using Configuration=SliderConfiguration;using Type=SliderType;using Rect=SliderRect;
 using EndpointWidths=SliderEndpointWidths;using Color=StyleColor;using IconCache=NativeIconCache<Decoder>;
 inline static constexpr auto symbol_size=slider_symbol_defaults::size;
 inline static constexpr auto symbol_icon=slider_symbol_defaults::icon,symbol_legacy_icon=slider_symbol_defaults::legacy_icon;
 inline static constexpr auto endpoint_font_size=slider_endpoint_defaults::font_size;
 inline static constexpr auto endpoint_font_weight=slider_endpoint_defaults::font_weight;
 inline static constexpr auto endpoint_line_height=slider_endpoint_defaults::line_height;
 inline static constexpr auto blur_radius=slider_filled_defaults::blur_radius,highlight_opacity=slider_filled_defaults::highlight_opacity;
 inline static constexpr auto highlight_offset_y=slider_filled_defaults::highlight_offset_y,ring_outset=slider_filled_defaults::ring_outset;
 inline static constexpr auto pressed_ring_stroke=slider_filled_defaults::pressed_ring_stroke,hover_ring_stroke=slider_filled_defaults::hover_ring_stroke;
 inline static constexpr auto maximum_tick_diameter=slider_segmented_defaults::maximum_tick_diameter,tick_track_divisor=slider_segmented_defaults::tick_track_divisor;
 inline static constexpr auto tick_size=slider_tick_defaults::size,tick_gap=slider_tick_defaults::gap,center_radius=slider_center_defaults::radius;
 inline static constexpr Color symbol_reference_color=figma_defaults::material_alert_action_border;
 [[nodiscard]] static bool has_filled_track(Type value)noexcept{return slider_has_filled_track(value);}
};
template<template<class>class NativeModel,template<class>class NativeHost,class Decoder>
using SliderViewBinding=NativeHost<SliderHostPolicy<NativeModel<SliderModelPolicy>,Decoder>>;
} // namespace lvui::native
