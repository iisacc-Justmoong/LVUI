#pragma once
#include <LVUI/ListItem.h>
#include <LVUI/NativeVisual.h>

namespace lvui::native {
// Only authored LVUI values and design metrics are adapted here; iiGUI owns
// row validation, geometry, accessibility projection and drawing.
struct ListItemModelPolicy final {
    using Configuration=ListItemConfiguration;using Variant=ListItemVariant;using Size=ListItemSize;
    using Rect=ListItemRect;using Geometry=ListItemGeometry;using Interaction=ListItemInteraction;
    using Visual=ListItemVisual;using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view value) noexcept{return valid_accessibility_text(value);}
    [[nodiscard]] static bool validate_extra(const Configuration&) noexcept{return true;}
    [[nodiscard]] static double value_line_height(const Configuration&) noexcept{return list_item_navigation_tokens::value_line_height;}
    [[nodiscard]] static double value_gap(const Configuration&) noexcept{return list_item_navigation_tokens::value_gap;}
};
template<template<class> class Model>using ListItemModelBinding=Model<ListItemModelPolicy>;
template<class NativeModel,class Converter>
struct ListItemDrawingPolicy final {
    using Model=NativeModel;
    using Variant=ListItemVariant;using Rect=ListItemRect;
    [[nodiscard]] static auto encoded_color(StyleColor value) noexcept{return Converter::convert(value);}
    [[nodiscard]] static double value_font_size(const ListItemConfiguration&) noexcept{return list_item_navigation_tokens::value_font_size;}
    [[nodiscard]] static double value_line_height(const ListItemConfiguration&) noexcept{return list_item_navigation_tokens::value_line_height;}
};
template<template<class> class Drawing,class Model,class Converter>
using ListItemDrawingBinding=Drawing<ListItemDrawingPolicy<Model,Converter>>;
} // namespace lvui::native
