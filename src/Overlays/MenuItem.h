#pragma once
#include <LVUI/Menu.h>
#include <LVUI/StyleTokens.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace lvui {

namespace shortcut_detail {
// Compare a key name without changing non-ASCII bytes in authored shortcuts.
[[nodiscard]] inline bool equal_ascii(std::string_view left,std::string_view right)noexcept{
    if(left.size()!=right.size())return false;
    for(std::size_t i=0;i<left.size();++i){
        auto c=left[i];if(c>='A'&&c<='Z')c=char(c-'A'+'a');
        if(c!=right[i])return false;
    }
    return true;
}
// Whitespace adjacent to a separator is presentation, not part of a key name.
[[nodiscard]] inline std::string_view trim(std::string_view value)noexcept{
    while(!value.empty()&&(value.front()==' '||value.front()=='\t'))value.remove_prefix(1);
    while(!value.empty()&&(value.back()==' '||value.back()=='\t'))value.remove_suffix(1);
    return value;
}
// Return the keyboard symbol belonging to a modifier, or an empty view.
[[nodiscard]] inline std::string_view modifier(std::string_view name)noexcept{
    if(name=="⇧"||name=="⌃"||name=="⌥"||name=="⌘"||name=="⊞")return name;
    if(equal_ascii(name,"shift"))return "⇧";
    if(equal_ascii(name,"control")||equal_ascii(name,"ctrl"))return "⌃";
    if(equal_ascii(name,"option")||equal_ascii(name,"alt"))return "⌥";
    if(equal_ascii(name,"command")||equal_ascii(name,"cmd"))return "⌘";
    if(equal_ascii(name,"windows")||equal_ascii(name,"win")||equal_ascii(name,"super"))return "⊞";
    if(equal_ascii(name,"meta")){
#if defined(__APPLE__)
        return "⌘";
#else
        return "⊞";
#endif
    }
    return {};
}
// Return common special-key symbols while leaving named function keys as text.
[[nodiscard]] inline std::string_view special_key(std::string_view name)noexcept{
    if(equal_ascii(name,"escape")||equal_ascii(name,"esc"))return "⎋";
    if(equal_ascii(name,"return")||equal_ascii(name,"enter"))return "↩";
    if(equal_ascii(name,"backspace"))return "⌫";
    if(equal_ascii(name,"delete")||equal_ascii(name,"del"))return "⌦";
    if(equal_ascii(name,"tab"))return "⇥";
    if(equal_ascii(name,"backtab"))return "⇤";
    if(equal_ascii(name,"space"))return "␣";
    if(equal_ascii(name,"capslock")||equal_ascii(name,"caps lock"))return "⇪";
    if(equal_ascii(name,"plus"))return "+";
    if(equal_ascii(name,"up"))return "↑";
    if(equal_ascii(name,"down"))return "↓";
    if(equal_ascii(name,"left"))return "←";
    if(equal_ascii(name,"right"))return "→";
    return {};
}
// Format one unambiguous chord; leave malformed and unfamiliar combinations intact.
[[nodiscard]] inline std::string chord(std::string_view authored){
    const auto body=trim(authored);
    if(body.empty())return std::string(authored);
    const auto leading=authored.substr(0,std::size_t(body.data()-authored.data()));
    const auto trailing=authored.substr(leading.size()+body.size());
    auto rest=body;
    const bool plus_key=rest.size()>=2&&rest.ends_with("++");
    if(plus_key)rest.remove_suffix(1);
    std::vector<std::string_view> modifiers;
    while(true){
        const auto separator=rest.find('+');if(separator==std::string_view::npos)break;
        const auto token=trim(rest.substr(0,separator));
        const auto symbol=modifier(token);
        if(symbol.empty())return std::string(authored);
        modifiers.push_back(symbol);rest.remove_prefix(separator+1);
    }
    const auto key=plus_key?std::string_view("+"):trim(rest);
    if(key.empty())return std::string(authored);
    auto symbol=modifier(key);if(symbol.empty())symbol=special_key(key);
    if(modifiers.empty()&&symbol.empty())return std::string(authored);
    std::string result(leading);
    for(const auto icon:modifiers)result.append(icon);
    result.append(symbol.empty()?key:symbol);
    result.append(trailing);
    return result;
}
} // namespace shortcut_detail

// Convert shortcut display text into keyboard symbols without changing key bindings.
// Comma-space-separated chords are formatted independently; unknown text is preserved.
[[nodiscard]] inline std::string format_shortcut(std::string_view authored){
    std::string result;
    while(true){
        const auto separator=authored.find(", ");
        result+=shortcut_detail::chord(authored.substr(0,separator));
        if(separator==std::string_view::npos)break;
        result+=", ";authored.remove_prefix(separator+2);
    }
    return result;
}

enum class MenuDirection { automatic,right,left,up,down };
struct MenuItemPalette final {
    StyleColor text{figma_defaults::text_title_header_color},disabled_text{1,1,1,.3},detail{1,1,1,.6};
    StyleColor selected{figma_defaults::accent_blue_selected},hover{figma_defaults::panel_surface_alt};
    StyleColor pressed{figma_defaults::accent_blue_selected},focus{figma_defaults::accent_blue_default};
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
