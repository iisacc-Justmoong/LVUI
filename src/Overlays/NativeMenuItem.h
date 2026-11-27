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

// Decorate the iiGUI model at the LVUI boundary so every menu construction
// path shares one shortcut presentation policy without changing its input.
template<class NativeModel>class ShortcutMenuItemModel final {
    NativeModel model_;
public:
    // Validate authored values using the underlying native model's contract.
    [[nodiscard]] static bool validate(const MenuItemConfiguration& value)noexcept{return NativeModel::validate(value);}

    // Keep the authored shortcut readable to assistive technology while the
    // native model measures and paints the corresponding keyboard symbols.
    bool set_configuration(MenuItemConfiguration value){
        if(!validate(value))return false;
        if(value.key_visible){
            auto& authored=value.shortcut.empty()?value.key_placeholder:value.shortcut;
            auto display=format_shortcut(authored);
            if(display!=authored&&display.size()<=65536){
                auto accessible=value.accessibility_name.empty()?(value.label.empty()?std::string("Menu item"):value.label):value.accessibility_name;
                if(accessible.size()+2+authored.size()<=65536){
                    accessible+=", ";accessible+=authored;value.accessibility_name=std::move(accessible);
                }
                authored=std::move(display);
            }
        }
        return model_.set_configuration(std::move(value));
    }

    // Return the prepared drawing configuration, including its accessible name.
    [[nodiscard]] const MenuItemConfiguration& configuration()const noexcept{return model_.configuration();}
    // Return the shortcut that the host measures and paints.
    [[nodiscard]] std::string_view shortcut()const noexcept{return model_.shortcut();}
    // Use the native model's resolved menu-item width.
    [[nodiscard]] double width()const noexcept{return model_.width();}
    // Use the native model's resolved menu-item height.
    [[nodiscard]] double height()const noexcept{return model_.height();}
    // Use the native model's submenu direction and expansion rules.
    [[nodiscard]] MenuDirection direction()const noexcept{return model_.direction();}
    // Delegate interaction precedence and palette selection to iiGUI.
    [[nodiscard]] MenuItemVisual visual(MenuItemInteraction interaction)const noexcept{return model_.visual(interaction);}
    // Delegate placement of measured content to the native layout model.
    [[nodiscard]] std::optional<MenuItemGeometry> geometry(double width,double height,double label_width,double label_height,
                                                           double shortcut_width,double shortcut_height)const noexcept{
        return model_.geometry(width,height,label_width,label_height,shortcut_width,shortcut_height);
    }
};
template<class NativeModel,class Decoder,class Widget,class Snapshot>struct MenuItemHostPolicy final {
    using Model=ShortcutMenuItemModel<NativeModel>;using Input=MenuItemInputValue<typename Decoder::Image>;using Callbacks=NativeMenuItemCallbacks;
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
