#pragma once
#include <LVUI/MenuItem.h>
#include <LVUI/NativeIconCache.h>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <vector>
namespace lvui::native {
[[nodiscard]] inline MenuEntryState menu_item_state(MenuEntryState value)noexcept{return value;}
template<class State> requires std::is_enum_v<State>
[[nodiscard]] inline MenuEntryState menu_item_state(State value)noexcept{
    if(value==State::normal)return MenuEntryState::normal;if(value==State::selected)return MenuEntryState::selected;
    if(value==State::inactive)return MenuEntryState::inactive;return static_cast<MenuEntryState>(-1);
}
[[nodiscard]] inline MenuDirection menu_item_direction(MenuDirection value)noexcept{return value;}
template<class Direction> requires std::is_enum_v<Direction>
[[nodiscard]] inline MenuDirection menu_item_direction(Direction value)noexcept{
    if(value==Direction::automatic)return MenuDirection::automatic;if(value==Direction::right)return MenuDirection::right;
    if(value==Direction::left)return MenuDirection::left;if(value==Direction::up)return MenuDirection::up;
    if(value==Direction::down)return MenuDirection::down;return static_cast<MenuDirection>(-1);
}
template<class Image>struct MenuItemInputValue final {
    MenuItemConfiguration configuration;std::shared_ptr<const Image> icon;std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision=0;bool visible=true;std::optional<bool> close_on_trigger;
    friend bool operator==(const MenuItemInputValue&,const MenuItemInputValue&)=default;
};
struct NativeMenuItemCallbacks final {std::function<void()> clicked,pressed,released,canceled;};
struct MenuItemPresentation final {
    std::optional<bool> selected,compact,show_icon_slot;std::optional<double> width;
    friend bool operator==(const MenuItemPresentation&,const MenuItemPresentation&)=default;
};
using MenuItemActivation=std::function<void(std::function<void()>)>;
using MenuCloseAction=std::function<ii::core::Result<void>()>;
using MenuClosePlan=std::function<ii::core::Result<MenuCloseAction>()>;
template<class Widget,class Snapshot>struct MenuPopupPortValue final {
    std::function<ii::core::Result<Snapshot>()> snapshot;
    std::function<ii::core::Result<void>(Widget,MenuDirection,MenuCloseAction)> open;
    std::function<ii::core::Result<void>()> close;
};
template<class Widget,class Snapshot>struct MenuItemPortValue final {
    std::function<ii::core::Result<void>(MenuItemPresentation)> present;
    std::function<ii::core::Result<void>(MenuItemActivation)> activated;
    std::function<ii::core::Result<void>(std::function<ii::core::Result<void>()>)> changed;
    std::function<ii::core::Result<MenuEntry>()> entry;
    std::function<ii::core::Result<Snapshot>()> snapshot;
    std::function<ii::core::Result<void>()> focus;
    std::function<ii::core::Result<bool>()> open_submenu;
    std::function<ii::core::Result<void>(MenuClosePlan)> parent_close;
    std::function<bool()> retired;
    std::function<ii::core::Result<bool>()> submenu_open;
    std::function<ii::core::Result<void>()> close_submenu;
    std::function<ii::core::Result<Widget>()> widget;
    std::vector<Widget> layout_groups;
};
template<class NativeModel,class Decoder,class Widget,class Snapshot>struct MenuItemHostPolicy final {
    using Model=NativeModel;using Input=MenuItemInputValue<typename Decoder::Image>;using Callbacks=NativeMenuItemCallbacks;
    using Presentation=MenuItemPresentation;using Activation=MenuItemActivation;using CloseAction=MenuCloseAction;using ClosePlan=MenuClosePlan;
    using PopupPort=MenuPopupPortValue<Widget,Snapshot>;using ItemPort=MenuItemPortValue<Widget,Snapshot>;
    using Entry=MenuEntry;using EntryKind=MenuEntryKind;using EntryState=MenuEntryState;using Direction=MenuDirection;
    using Rect=MenuRect;using Color=StyleColor;using IconCache=NativeIconCache<Decoder>;
    [[nodiscard]] static std::string_view chevron_name(Direction value)noexcept {
        switch(value){case Direction::right:return "generalchevronRight";case Direction::left:return "generalchevronLeft";
        case Direction::up:return "generalchevronUp";case Direction::down:return "generalchevronDown";default:return {}; }
    }
};
template<template<class>class NativeModel,template<class>class NativeHost,class Decoder,class Widget,class Snapshot>
using MenuItemBinding=NativeHost<MenuItemHostPolicy<NativeModel<MenuItemModelPolicy>,Decoder,Widget,Snapshot>>;
} // namespace lvui::native
