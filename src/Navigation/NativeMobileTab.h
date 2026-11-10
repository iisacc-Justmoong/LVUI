#pragma once
#include <LVUI/NativeMobileNavigation.h>
#include <memory>

namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor mobile_tab_color(const Color& c)noexcept{return {c.red,c.green,c.blue,c.alpha};}
// Declarative inputs and injected asset contract; iiGUI owns tab rendering.
struct MobileTabInput final {
    MobileNavigationItem item;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision=0;
    double width=mobile_navigation_tokens::tab_width,height=mobile_navigation_tokens::touch_size;
    bool selected=false,enabled=true,show_label=true,draw_selection=true,motion_enabled=true;
    MobileNavigationPlatform platform=MobileNavigationPlatform::automatic;
    StyleColor focus_color{figma_defaults::accent_blue_default};
    friend bool operator==(const MobileTabInput&,const MobileTabInput&)=default;
};
template<class Decoder>struct MobileTabHostPolicy final {
    using Input=MobileTabInput;using Item=MobileNavigationItem;using Platform=MobileNavigationPlatform;
    using Color=StyleColor;using Tokens=MobileNavigationTokens;using Colors=MobileNavigationColors;
    using IconCache=NativeIconCache<Decoder>;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<template<class>class Host,class Decoder>using MobileTabBinding=Host<MobileTabHostPolicy<Decoder>>;
} // namespace lvui::native
