#pragma once

#include <LVUI/MenuItem.h>

#include <any>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lvui {

struct ListItemMenuSelection;

// Applications own the meaning and lifetime of a menu command.
class ListItemMenuCommand {
public:
    virtual ~ListItemMenuCommand()=default;
    virtual void invoke(const ListItemMenuSelection&)const=0;
};

// User data is an immutable, type-erased application value. Its shared
// identity also makes reactive menu updates deterministic without knowing T.
struct ListItemMenuEntry final {
    std::string label,shortcut,icon_name,accessibility_name;
    bool enabled=true,visible=true,divider=false,expanded=false;
    MenuEntryState state=MenuEntryState::normal;
    MenuDirection direction=MenuDirection::automatic;
    std::optional<bool> show_icon_slot,show_shortcut,show_chevron,has_child_items,close_on_trigger;
    std::shared_ptr<const std::any> payload;
    std::shared_ptr<const ListItemMenuCommand> command;
    std::vector<ListItemMenuEntry> children;
    friend bool operator==(const ListItemMenuEntry&,const ListItemMenuEntry&)=default;
};

// An independent validation policy shared by native and non-native consumers.
struct ListItemMenuPolicy final {
    using ListItemMenuEntry=::lvui::ListItemMenuEntry;
    using MenuEntryState=::lvui::MenuEntryState;
    using MenuDirection=::lvui::MenuDirection;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<template<class>class Engine>using ListItemMenuBinding=Engine<ListItemMenuPolicy>;

} // namespace lvui
