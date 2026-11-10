#pragma once
#include <LVUI/Text.h>
#include <LVUI/StyleTokens.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace lvui {
enum class MenuEntryKind { action, submenu, divider };
enum class MenuEntryState { normal,selected,inactive };
enum class MenuNavigation { first,last,previous,next };
enum class MenuErrorCode { invalid,duplicate,limit,closed,unavailable,stale };
struct MenuError final {MenuErrorCode code;std::string message;};
template<class T>using MenuResult=std::expected<T,MenuError>;
struct MenuEntry final {
    std::string key,label,shortcut,icon_name,accessibility_name;
    MenuEntryKind kind=MenuEntryKind::action;
    MenuEntryState state=MenuEntryState::normal;
    bool enabled=true,visible=true,compact=true;
    std::optional<double> height;
    std::optional<bool> close_on_trigger;
    friend bool operator==(const MenuEntry&,const MenuEntry&)=default;
};
struct MenuConfiguration final {
    double item_width=141,item_spacing=2,left_padding=8,right_padding=8,top_padding=8,bottom_padding=8;
    double corner_radius=8,opacity=.12,edge_margin=4;
    bool auto_close_on_trigger=true,compact_items=true,show_icon_slot=true,dismiss_outside=true,dismiss_context=true;
    std::string accessibility_name;
    StyleColor base{figma_defaults::panel_window},background{.14,.14,.14,1},border{1,1,1,.12};
    friend bool operator==(const MenuConfiguration&,const MenuConfiguration&)=default;
};
struct MenuRect final {double x=0,y=0,width=0,height=0;};
struct MenuLayout final {double width=0,height=0;std::vector<MenuRect> rows;};
struct MenuActivation final {
    std::uint64_t epoch=0,revision=0;
    std::size_t index=0;
    std::string key;
    bool close_on_complete=false;
};

// Values and text validation are supplied to the application-composed iiGUI model.
struct MenuModelPolicy final {
    using Configuration=MenuConfiguration;using Entry=MenuEntry;using EntryKind=MenuEntryKind;using EntryState=MenuEntryState;
    using Navigation=MenuNavigation;using Error=MenuError;using ErrorCode=MenuErrorCode;
    using Activation=MenuActivation;using Layout=MenuLayout;using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
} // namespace lvui
