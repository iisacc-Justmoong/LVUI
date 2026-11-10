#pragma once
#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lvui {
enum class TabStyle { underline, surface };
struct TabPalette final {
    StyleColor selected{1,1,1,1};
    StyleColor text{1,1,1,.8};
    StyleColor disabled_text{1,1,1,.3};
    StyleColor badge{1,1,1,.6};
    StyleColor surface_selected{.20,.20,.20,1};
    StyleColor hover{.12,.12,.12,1};
    StyleColor pressed{.16,.16,.16,1};
    StyleColor accent{figma_defaults::accent_blue_default};
    friend bool operator==(const TabPalette&,const TabPalette&)=default;
};
struct TabConfiguration final {
    std::string text,accessibility_name,icon_name,badge;
    bool enabled=true,selected=false;
    TabStyle style=TabStyle::underline;
    double height=32,minimum_width=80,icon_size=18,horizontal_padding=12,spacing=8;
    double font_size=13,line_height=13;
    unsigned font_weight=500;
    TabPalette palette;
    friend bool operator==(const TabConfiguration&,const TabConfiguration&)=default;
};
struct TabInteraction final {bool enabled=true,hovered=false,pressed=false,focused=false;};
struct TabVisual final {StyleColor background,text,badge,accent;bool focused=false,underline=false;};
struct TabRect final {double x=0,y=0,width=0,height=0;};
struct TabGeometry final {TabRect surface,icon,label,badge,underline;double radius=0;};

struct TabModelPolicy final {
    using Configuration=TabConfiguration;using Interaction=TabInteraction;using Visual=TabVisual;
    using Geometry=TabGeometry;using Style=TabStyle;using Color=StyleColor;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};

struct TabBarConfiguration final {
    double spacing=4,minimum_tab_width=120;
    int current_index=0;
    bool auto_select=true;
    std::string accessibility_name;
    friend bool operator==(const TabBarConfiguration&,const TabBarConfiguration&)=default;
};
enum class TabFocusMove { first,last,previous,next };
enum class TabActivation { rejected,invoked,selection_changed };
struct TabBarModelPolicy final {
    using Configuration=TabBarConfiguration;using FocusMove=TabFocusMove;using Activation=TabActivation;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};

} // namespace lvui
