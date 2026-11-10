#pragma once
#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
enum class PopoverPlacement { anchored, centered };
struct PopoverSize final {double width=0,height=0;friend bool operator==(const PopoverSize&,const PopoverSize&)=default;};
struct PopoverConfiguration final {
    double width=0,height=0,padding=12,margins=8,corner_radius=12,surface_opacity=.25;
    std::optional<StyleColor> surface_color,primary_color;
    bool modal=false,dim=false,focus_on_open=true,clip_content=false,dismiss_on_escape=true,dismiss_on_outside_press=true;
    std::optional<bool> dismiss_on_context_request;
    std::string accessibility_name="Popover";
    PopoverPlacement placement=PopoverPlacement::anchored;
    bool blur_enabled=true,motion_enabled=true;
    friend bool operator==(const PopoverConfiguration&,const PopoverConfiguration&)=default;
};

// The app injects design values and text validation into the iiGUI model.
struct PopoverModelPolicy final {
    using Configuration=PopoverConfiguration;using Size=PopoverSize;using Placement=PopoverPlacement;using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
} // namespace lvui
