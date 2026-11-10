#pragma once
#include <LVUI/MobileNavigationTokens.generated.h>
#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <cmath>
#include <concepts>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace lvui {
enum class MobileNavigationPlatform { automatic, ios, android };
[[nodiscard]] inline MobileNavigationPlatform mobile_navigation_platform(MobileNavigationPlatform value){
    switch(value){case MobileNavigationPlatform::automatic:case MobileNavigationPlatform::ios:case MobileNavigationPlatform::android:return value;}
    throw std::invalid_argument("Invalid mobile navigation platform");
}
template<std::integral T> requires (!std::same_as<std::remove_cv_t<T>,bool>)
[[nodiscard]] inline MobileNavigationPlatform mobile_navigation_platform(T value){
    if(value==0)return MobileNavigationPlatform::automatic;
    if(value==1)return MobileNavigationPlatform::ios;
    if(value==2)return MobileNavigationPlatform::android;
    throw std::invalid_argument("Invalid mobile navigation platform");
}
struct MobileNavigationItem final {
    std::string key,text,icon_name,accessibility_name;
    bool enabled=true;
    std::string badge;
    bool badge_dot=false;
    std::string badge_accessibility_name;
    friend bool operator==(const MobileNavigationItem&,const MobileNavigationItem&)=default;
};
struct MobileNavigationConfiguration final {
    std::vector<MobileNavigationItem> items;
    std::optional<MobileNavigationItem> search;
    double width=402,bottom_safe_inset=0;
    int current_index=-1;
    bool auto_select=true,enabled=true;
    bool material_enabled=false,motion_enabled=true;
    MobileNavigationPlatform platform=MobileNavigationPlatform::automatic;
    StyleColor focus_color{figma_defaults::accent_blue_default};
    std::string accessibility_name="Main navigation";
    friend bool operator==(const MobileNavigationConfiguration&,const MobileNavigationConfiguration&)=default;
};
struct MobileNavigationLayout final {
    double height=0,group_x=0,group_y=0,group_width=0,viewport_width=0,content_width=0,tab_width=0,search_x=0;
    friend bool operator==(const MobileNavigationLayout&,const MobileNavigationLayout&)=default;
};
struct MobileNavigationModelPolicy final {
 using Configuration=MobileNavigationConfiguration;using Item=MobileNavigationItem;using Layout=MobileNavigationLayout;using Platform=MobileNavigationPlatform;
 inline static constexpr auto outer=mobile_navigation_tokens::outer,group_height=mobile_navigation_tokens::group_height,gap=mobile_navigation_tokens::gap;
 inline static constexpr auto inset=mobile_navigation_tokens::inset,tab_width=mobile_navigation_tokens::tab_width,touch_size=mobile_navigation_tokens::touch_size;
 [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<template<class>class Model>using MobileNavigationModelBinding=Model<MobileNavigationModelPolicy>;

} // namespace lvui
