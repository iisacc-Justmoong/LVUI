#pragma once
#include <LVUI/WindowAccent.h>
#include <LVUI/ListItemSelector.h>
#include <LVUI/ListItem.h>
#include <LVUI/NativeComboBox.h>
#include <LVUI/NativeMenu.h>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>
namespace lvui::native {
[[nodiscard]] inline ListItemSelectorConfiguration list_item_selector(ListItemSelectorConfiguration value){return value;}
[[nodiscard]] inline ListItemSelectorConfiguration list_item_selector(std::vector<ListItemOption> options){
    ListItemSelectorConfiguration value;value.options=std::move(options);return value;
}
[[nodiscard]] inline ListItemSelectorConfiguration list_item_selector(const std::vector<std::string>& options){
    ListItemSelectorConfiguration value;value.options.reserve(options.size());
    for(const auto& text:options)value.options.push_back({text,true});return value;
}
[[nodiscard]] inline std::string list_item_selector_placeholder(ListItemVariant variant){
    return variant==ListItemVariant::detail_quantity?"Format":variant==ListItemVariant::detail_settings?"Automatic":
        variant==ListItemVariant::task?"Priority":variant==ListItemVariant::form?"Category":"Option";
}
[[nodiscard]] inline ListItemSelectorConfiguration list_item_selector(ListItemSelectorConfiguration value,ListItemVariant){return value;}
[[nodiscard]] inline ListItemSelectorConfiguration list_item_selector(std::vector<ListItemOption> options,ListItemVariant variant){
    auto value=list_item_selector(std::move(options));value.placeholder=list_item_selector_placeholder(variant);return value;
}
[[nodiscard]] inline ListItemSelectorConfiguration list_item_selector(const std::vector<std::string>& options,ListItemVariant variant){
    auto value=list_item_selector(options);value.placeholder=list_item_selector_placeholder(variant);return value;
}
[[nodiscard]] inline ListItemSelectorConfiguration list_item_unit_selector(ListItemSelectorConfiguration value){return value;}
[[nodiscard]] inline ListItemSelectorConfiguration list_item_unit_selector(std::vector<ListItemOption> options){
    auto value=list_item_selector(std::move(options));value.placeholder="Units";return value;
}
[[nodiscard]] inline ListItemSelectorConfiguration list_item_unit_selector(const std::vector<std::string>& options){
    auto value=list_item_selector(options);value.placeholder="Units";return value;
}

struct ListItemSelectorInput final {
    ListItemSelectorConfiguration configuration;
    std::optional<WindowAccentPalette> accent;
    double width=97,height=20;
    bool visible=true;
    std::string accessibility_name;
    friend bool operator==(const ListItemSelectorInput&,const ListItemSelectorInput&)=default;
};

template<class NativeModel,class Combo,class Menu,class Item,class Image>
struct ListItemSelectorHostPolicy final {
 using Model=NativeModel;using Input=ListItemSelectorInput;using NativeComboBox=Combo;using NativeMenu=Menu;using NativeMenuItem=Item;
 using MenuItemInput=MenuItemInputValue<Image>;
 [[nodiscard]] static bool valid_accent(const std::optional<WindowAccentPalette>& accent)noexcept{return !accent||valid_window_accent_palette(*accent);}
 [[nodiscard]] static bool valid_text(std::string_view text)noexcept{return valid_accessibility_text(text);}
 [[nodiscard]] static ComboBoxConfiguration control_input(const Input& input,const Model& model){
  ComboBoxConfiguration value;value.text=model.display_text();value.accessibility_name=input.accessibility_name;value.width=input.width;value.height=input.height;
  value.enabled=model.configuration().enabled;if(const auto& accent=input.accent)value.palette.focus=accent->primary;value.arrow=StepperArrow::down;return value;}
 static void accent_item(MenuItemInput& item,const std::optional<WindowAccentPalette>& accent){const MenuItemPalette defaults;
  item.configuration.palette.selected=accent?accent->primary:defaults.selected;item.configuration.palette.pressed=accent?accent->subtle:defaults.pressed;
  item.configuration.palette.focus=accent?accent->primary:defaults.focus;}
 [[nodiscard]] static MenuInput menu_input(const Input& input,const Model& model){MenuInput value;value.configuration.item_width=input.width;
  value.configuration.show_icon_slot=false;value.configuration.accessibility_name=input.accessibility_name;const auto& c=model.configuration();
  value.selected_index=!c.options.empty()&&c.options[std::size_t(c.selected_index)].enabled?c.selected_index:-1;return value;}
};
template<template<class>class NativeModel,template<class>class NativeHost,class Combo,class Menu,class Item,class Image>
using ListItemSelectorBinding=NativeHost<ListItemSelectorHostPolicy<NativeModel<ListItemSelectorModelPolicy>,Combo,Menu,Item,Image>>;
} // namespace lvui::native
