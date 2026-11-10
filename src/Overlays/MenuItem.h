#pragma once
#include <LVUI/Menu.h>
#include <LVUI/StyleTokens.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
namespace lvui {
enum class MenuDirection { automatic,right,left,up,down };
struct MenuItemPalette final {
    StyleColor text{1,1,1,.8},disabled_text{1,1,1,.3},detail{1,1,1,.6};
    StyleColor selected{figma_defaults::accent_blue_default},hover{figma_defaults::panel_surface_alt};
    StyleColor pressed{figma_defaults::accent_blue_subtle},focus{figma_defaults::accent_blue_default};
    friend bool operator==(const MenuItemPalette&,const MenuItemPalette&)=default;
};
struct MenuItemConfiguration final {
    std::string label="Label",shortcut,key_placeholder,icon_name="procedure",accessibility_name;
    MenuEntryState state=MenuEntryState::normal;
    MenuDirection direction=MenuDirection::automatic;
    bool enabled=true,compact=false,key_visible=true,show_icon_slot=true,show_chevron=true,has_children=true,expanded=false;
    double width=0,height=0,icon_size=16,chevron_size=16,horizontal_padding=4,spacing=8;
    double font_size=13,line_height=13;
    unsigned font_weight=500;
    MenuItemPalette palette;
    friend bool operator==(const MenuItemConfiguration&,const MenuItemConfiguration&)=default;
};
struct MenuItemInteraction final {bool enabled=true,hovered=false,pressed=false,focused=false;};
struct MenuItemVisual final {StyleColor background,text,detail,focus;bool enabled=true,focused=false;};
struct MenuItemGeometry final {MenuRect surface,icon,label,shortcut,chevron;double radius=0;};

// Design values and text policy are supplied to the application-composed iiGUI model.
struct MenuItemModelPolicy final {
    using Configuration=MenuItemConfiguration;using Interaction=MenuItemInteraction;using Visual=MenuItemVisual;
    using Geometry=MenuItemGeometry;using State=MenuEntryState;using Direction=MenuDirection;using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
} // namespace lvui
