#pragma once
#include <LVUI/StyleTokens.h>
#include <LVUI/SliderTicks.generated.h>
#include <LVUI/SliderCenter.generated.h>
#include <LVUI/SliderFilled.generated.h>
#include <LVUI/SliderSegmented.generated.h>
#include <LVUI/SliderSymbol.generated.h>
#include <LVUI/SliderEndpointTokens.generated.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace lvui {
enum class SliderSize { mini, small, regular, large };
enum class SliderType { normal, center_biased, ticks, center_biased_ticks, filled, min_max_labels, segmented };
inline constexpr bool slider_has_filled_track(SliderType value) noexcept {
    return value==SliderType::filled||value==SliderType::min_max_labels||value==SliderType::segmented;
}
inline constexpr bool slider_has_labeled_preset(SliderType value) noexcept {
    return value==SliderType::min_max_labels||value==SliderType::segmented;
}
inline SliderType slider_type(SliderType value) {
    switch(value){case SliderType::normal:case SliderType::center_biased:
        case SliderType::ticks:case SliderType::center_biased_ticks:case SliderType::filled:
        case SliderType::min_max_labels:case SliderType::segmented:return value;}
    throw std::invalid_argument("Invalid or unsupported Slider type");
}
template<std::integral T> requires (!std::same_as<std::remove_cv_t<T>,bool>)
SliderType slider_type(T value) {
    if(value==0)return SliderType::normal;if(value==1)return SliderType::center_biased;
    if(value==2)return SliderType::ticks;if(value==3)return SliderType::center_biased_ticks;
    if(value==4)return SliderType::filled;if(value==5)return SliderType::min_max_labels;
    if(value==6)return SliderType::segmented;
    throw std::invalid_argument("Invalid or unsupported Slider type");
}
inline std::size_t slider_tick_count(double value) {
    if(!std::isfinite(value)||value<2||value>512||std::trunc(value)!=value)
        throw std::invalid_argument("Slider tick count must be a whole number from 2 to 512");
    return static_cast<std::size_t>(value);
}
inline std::size_t slider_segment_count(double value) {
    if(!std::isfinite(value)||value<2||value>512||std::trunc(value)!=value)
        throw std::invalid_argument("Slider segment count must be a whole number from 2 to 512");
    return static_cast<std::size_t>(value);
}
inline SliderSize slider_size(SliderSize value) {
    switch(value){case SliderSize::mini:case SliderSize::small:case SliderSize::regular:case SliderSize::large:return value;}
    throw std::invalid_argument("Invalid Slider size");
}
template<std::integral T> requires (!std::same_as<std::remove_cv_t<T>,bool>)
SliderSize slider_size(T value) {
    if(value==0)return SliderSize::mini;if(value==1)return SliderSize::small;
    if(value==2)return SliderSize::regular;if(value==3)return SliderSize::large;
    throw std::invalid_argument("Invalid Slider size");
}
struct SliderPalette final {
    StyleColor track{figma_defaults::panel_surface_solid};
    StyleColor fill{figma_defaults::accent_blue_default};
    StyleColor thumb{figma_defaults::text_title_header_color};
    StyleColor focus{figma_defaults::accent_blue_default};
    StyleColor mark{slider_center_defaults::color};
    StyleColor label{slider_endpoint_defaults::label_color};
    StyleColor symbol{figma_defaults::material_alert_action_border};
    StyleColor inactive_fill{figma_defaults::text_disabled_color};
    friend bool operator==(const SliderPalette&,const SliderPalette&)=default;
};
struct SliderConfiguration final {
    double minimum=0,maximum=1,value=.5,step=0;
    double width=320,height=32,thumb_diameter=18,track_height=6,range_inset=9;
    bool enabled=true,show_ticks=false,show_symbol=false;
    bool show_min_max=false,show_labels=true,show_endpoint_icons=false,right_to_left=false;
    std::size_t tick_count=slider_tick_defaults::count;
    std::size_t segment_count=slider_segmented_defaults::default_segment_count;
    bool automatic_segment_step=true;
    SliderPalette palette;
    std::string symbol_name=slider_symbol_defaults::icon;
    std::string minimum_label=slider_endpoint_defaults::minimum_label,maximum_label=slider_endpoint_defaults::maximum_label;
    std::string minimum_icon_name=slider_symbol_defaults::icon,maximum_icon_name=slider_symbol_defaults::icon;
    std::string accessibility_name="Slider";
    bool center_biased=false;
    SliderType type=SliderType::normal;
    SliderSize size=SliderSize::regular;
    bool active=true;
    friend bool operator==(const SliderConfiguration&,const SliderConfiguration&)=default;
};
inline void apply_slider_size(SliderConfiguration& value,SliderSize size);
inline void apply_slider_type(SliderConfiguration& value,SliderType type) {
    type=slider_type(type);const bool was_filled=slider_has_filled_track(value.type);
    const bool was_labeled=slider_has_labeled_preset(value.type);
    value.type=type;
    switch(type){
        case SliderType::normal:value.center_biased=false;value.show_ticks=false;break;
        case SliderType::center_biased:value.center_biased=true;value.show_ticks=false;break;
        case SliderType::ticks:value.center_biased=false;value.show_ticks=true;break;
        case SliderType::center_biased_ticks:value.center_biased=true;value.show_ticks=true;break;
        case SliderType::filled:value.center_biased=false;value.show_ticks=false;break;
        case SliderType::min_max_labels:value.center_biased=false;value.show_ticks=false;break;
        case SliderType::segmented:value.center_biased=false;value.show_ticks=true;break;
    }
    if(slider_has_labeled_preset(type))value.show_min_max=true;
    else if(was_labeled)value.show_min_max=false;
    if(was_filled&&!slider_has_filled_track(type))value.show_symbol=false;
    if(slider_has_filled_track(type)||was_filled)apply_slider_size(value,value.size);
}
inline void apply_slider_size(SliderConfiguration& value,SliderSize size) {
    size=slider_size(size);value.size=size;
    if(slider_has_filled_track(value.type)){
        const auto& preset=slider_filled_defaults::sizes[static_cast<std::size_t>(size)];
        value.height=preset.height;value.track_height=preset.track_height;value.thumb_diameter=preset.thumb_diameter;
        value.range_inset=preset.range_inset;value.show_symbol=value.type==SliderType::filled&&preset.show_symbol;return;
    }
    switch(size){
        case SliderSize::mini:value.height=22;value.thumb_diameter=12;value.track_height=4;value.range_inset=8;break;
        case SliderSize::small:value.height=24;value.thumb_diameter=14;value.track_height=6;value.range_inset=8;break;
        case SliderSize::regular:value.height=32;value.thumb_diameter=18;value.track_height=6;value.range_inset=9;break;
        case SliderSize::large:value.height=44;value.thumb_diameter=22;value.track_height=8;value.range_inset=11;break;
    }
}
struct SliderRect final {double x=0,y=0,width=0,height=0;};
struct SliderGeometry final {SliderRect track,fill,thumb,center_mark;};
struct SliderEndpointWidths final {double minimum=0,maximum=0;};
struct SliderEndpointLayout final {SliderRect control,minimum_label,maximum_label,minimum_icon,maximum_icon;};

struct SliderModelPolicy final {
    using Configuration=SliderConfiguration;using Type=SliderType;using Size=SliderSize;using Color=StyleColor;
    using Rect=SliderRect;using Geometry=SliderGeometry;using EndpointWidths=SliderEndpointWidths;using EndpointLayout=SliderEndpointLayout;
    inline static constexpr auto endpoint_minimum_control_width=slider_endpoint_defaults::minimum_control_width;
    inline static constexpr auto endpoint_outer_gap=slider_endpoint_defaults::outer_gap,endpoint_inner_gap=slider_endpoint_defaults::inner_gap;
    inline static constexpr auto endpoint_line_height=slider_endpoint_defaults::line_height,symbol_size=slider_symbol_defaults::size;
    inline static constexpr auto center_width=slider_center_defaults::width,center_overhang=slider_center_defaults::overhang;
    [[nodiscard]] static bool valid_text(std::string_view text)noexcept{return valid_accessibility_text(text);}
    [[nodiscard]] static bool has_filled_track(Type type)noexcept{return slider_has_filled_track(type);}
};
template<template<class>class Implementation>using SliderBinding=Implementation<SliderModelPolicy>;

} // namespace lvui
