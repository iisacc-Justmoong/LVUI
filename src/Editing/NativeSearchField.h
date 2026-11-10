#pragma once
#include <LVUI/NativeIconCache.h>
#include <LVUI/NativeInputField.h>
#include <LVUI/SearchFieldTokens.generated.h>
#include <cmath>
#include <concepts>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
namespace lvui {
inline InputFieldSurface search_field_surface(InputFieldSurface value){
    if(value==InputFieldSurface::rounded||value==InputFieldSurface::cylinder||value==InputFieldSurface::inline_)return value;
    throw std::invalid_argument("Invalid search field surface");
}
template<std::integral T> requires (!std::same_as<std::remove_cv_t<T>,bool>)
InputFieldSurface search_field_surface(T value){return search_field_surface(input_field_surface(value));}
struct SearchFieldConfiguration final {
    std::string text,placeholder,accessibility_name="Search",clear_accessibility_name="Clear search";
    double width=InputFieldConfiguration{}.width;
    bool enabled=true,read_only=false,clear_button_visible=true;
    InputFieldSurface surface=InputFieldSurface::rounded;
    friend bool operator==(const SearchFieldConfiguration&,const SearchFieldConfiguration&)=default;
};
[[nodiscard]] inline bool valid_search_field_configuration(const SearchFieldConfiguration& value)noexcept{
    return std::isfinite(value.width)&&value.width>=1&&value.width<=8192&&
        (value.surface==InputFieldSurface::rounded||value.surface==InputFieldSurface::cylinder||value.surface==InputFieldSurface::inline_)&&
        valid_accessibility_text(value.text)&&valid_accessibility_text(value.placeholder)&&
        valid_accessibility_text(value.accessibility_name)&&valid_accessibility_text(value.clear_accessibility_name);
}
namespace native {
struct NativeSearchFieldCallbacks final {std::function<void(std::string)> edited;std::function<void()> submitted;};
struct NativeSearchFieldIds final {std::string editor,search_icon,clear;};
struct SearchFieldMetrics final {
 double height,icon,padding,vertical_padding,gap,radius,font_size,line_height,clear_line_height;
 StyleColor background,text,disabled_text,description,accent;std::string font;
};
template<class NativeField,class NativeMaterial,class Decoder,class Affinity>struct SearchFieldHostPolicy final {
 using Field=NativeField;using Configuration=SearchFieldConfiguration;using InputConfiguration=InputFieldConfiguration;
 using Surface=InputFieldSurface;using Color=StyleColor;using FieldCallbacks=NativeInputFieldCallbacks;
 using Callbacks=NativeSearchFieldCallbacks;using Ids=NativeSearchFieldIds;using Snapshot=NativeInputFieldSnapshotValue<Affinity>;
 using Material=NativeMaterial;using MaterialVariant=text_field_material::Variant;using IconCache=NativeIconCache<Decoder>;
 using Metrics=SearchFieldMetrics;
 inline static constexpr auto cylinder_radius=search_field_defaults::cylinder_radius;
 inline static constexpr std::string_view icon_name="searchField";
 [[nodiscard]] static Metrics metrics(){namespace d=::lvui::search_field_defaults;
  return {d::height,d::icon_size,d::horizontal_padding,d::vertical_padding,d::gap,d::radius,d::font_size,d::line_height,
   d::clear_line_height,d::background,d::text,d::disabled_text,d::description,d::accent,d::font_family};}
 [[nodiscard]] static bool valid_configuration(const Configuration& v)noexcept{return valid_search_field_configuration(v);}
 [[nodiscard]] static bool valid_text(std::string_view v)noexcept{return valid_accessibility_text(v);}
};
template<template<class>class Host,class Field,class Material,class Decoder,class Affinity>
using SearchFieldBinding=Host<SearchFieldHostPolicy<Field,Material,Decoder,Affinity>>;
} // namespace native
} // namespace lvui
