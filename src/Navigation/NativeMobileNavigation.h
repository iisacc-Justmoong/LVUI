#pragma once
#include <LVUI/MobileNavigation.h>
#include <LVUI/MobileNavigationMotion.h>
#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeMaterial.h>
#include <functional>
namespace lvui::native {
struct NativeMobileNavigationCallbacks final {std::function<void(int)> activated;std::function<void()> search_requested;};
struct MobileNavigationTokens final {
 inline static constexpr auto outer=mobile_navigation_tokens::outer;
 inline static constexpr auto gap=mobile_navigation_tokens::gap;
 inline static constexpr auto inset=mobile_navigation_tokens::inset;
 inline static constexpr auto group_height=mobile_navigation_tokens::group_height;
 inline static constexpr auto tab_width=mobile_navigation_tokens::tab_width;
 inline static constexpr auto touch_size=mobile_navigation_tokens::touch_size;
 inline static constexpr auto ios_radius=mobile_navigation_tokens::ios_radius;
 inline static constexpr auto ios_selected_radius=mobile_navigation_tokens::ios_selected_radius;
 inline static constexpr auto android_radius=mobile_navigation_tokens::android_radius;
 inline static constexpr auto android_selected_radius=mobile_navigation_tokens::android_selected_radius;
 inline static constexpr auto icon_size=mobile_navigation_tokens::icon_size;
 inline static constexpr auto label_size=mobile_navigation_tokens::label_size;
 inline static constexpr auto label_line_height=mobile_navigation_tokens::label_line_height;
 inline static constexpr auto icon_label_gap=mobile_navigation_tokens::icon_label_gap;
 inline static constexpr auto border_width=mobile_navigation_tokens::border_width;
 inline static constexpr auto badge_dot=mobile_navigation_tokens::badge_dot;
 inline static constexpr auto badge_height=mobile_navigation_tokens::badge_height;
 inline static constexpr auto badge_padding=mobile_navigation_tokens::badge_padding;
 inline static constexpr auto badge_offset=mobile_navigation_tokens::badge_offset;
};
struct MobileNavigationColors final {
 inline static constexpr auto panel_window_alt=figma_defaults::panel_window_alt;
 inline static constexpr auto panel_surface_panel=figma_defaults::panel_surface_panel;
 inline static constexpr auto panel_surface_input_hover=figma_defaults::panel_surface_input_hover;
 inline static constexpr auto panel_surface_input=figma_defaults::panel_surface_input;
 inline static constexpr auto text_disabled_color=figma_defaults::text_disabled_color;
 inline static constexpr auto text_title_header_color=figma_defaults::text_title_header_color;
 inline static constexpr auto text_body_color=figma_defaults::text_body_color;
 inline static constexpr auto accent_red_default=figma_defaults::accent_red_default;
 inline static constexpr auto material_glass_fill=figma_defaults::material_glass_fill;
};
template<class NativeModel,class NativeMotion,class NativeMaterial,class Decoder>struct MobileNavigationHostPolicy final {
 using Model=NativeModel;using Motion=NativeMotion;using Configuration=MobileNavigationConfiguration;using Item=MobileNavigationItem;
 using Platform=MobileNavigationPlatform;using Color=StyleColor;using Tokens=MobileNavigationTokens;using Colors=MobileNavigationColors;
 using Material=NativeMaterial;using MaterialConfiguration=lvui::MaterialConfiguration;using MaterialDensity=lvui::MaterialDensity;using MaterialBackdrop=lvui::MaterialBackdrop;
 using IconCache=NativeIconCache<Decoder>;using Callbacks=NativeMobileNavigationCallbacks;
};
template<template<class>class Host,class NativeModel,class NativeMotion,class NativeMaterial,class Decoder>
using MobileNavigationBinding=Host<MobileNavigationHostPolicy<NativeModel,NativeMotion,NativeMaterial,Decoder>>;
} // namespace lvui::native
