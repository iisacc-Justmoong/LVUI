#pragma once
#include <LVUI/Motion.h>
#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
enum class CheckBoxShape { round_rect, cylinder };
struct CheckBoxPalette final {
    StyleColor checked{figma_defaults::accent_blue_default},unchecked{1,1,1,.8};
    StyleColor disabled_checked{figma_defaults::panel_surface_raised};
    StyleColor disabled_unchecked{figma_defaults::panel_surface_raised};
    StyleColor mark{1,1,1,.8},disabled_mark{1,1,1,.3};
    StyleColor checked_border{0,0,0,0},unchecked_border{1,1,1,.8};
    StyleColor disabled_checked_border{figma_defaults::panel_surface_raised};
    StyleColor disabled_unchecked_border{0,0,0,0};
    StyleColor text{1,1,1,.8},disabled_text{1,1,1,.3},focus{figma_defaults::accent_blue_default};
    StyleColor inner_shadow_soft{0,0,0,20./255.},inner_shadow_strong{0,0,0,26./255.};
    friend bool operator==(const CheckBoxPalette&,const CheckBoxPalette&) = default;
};
struct CheckBoxConfiguration final {
    std::string text,accessibility_name;
    bool enabled=true,checked=false;
    double box_size=17,frame_padding=.5,box_radius=3.5,mark_stroke_width=2,label_gap=6;
    double checked_border_width=0,unchecked_border_width=.5;
    double disabled_checked_border_width=.5,disabled_unchecked_border_width=0;
    double font_size=13,line_height=13;
    unsigned font_weight=500;
    CheckBoxPalette palette;
    // Null follows the default-primary color; explicit values override that policy.
    std::optional<bool> use_figma_checked_assets;
    CheckBoxShape shape_style=CheckBoxShape::round_rect;
    bool motion_enabled=true;
    Motion motion{.duration=std::chrono::milliseconds{360},.curve=MotionCurve::out_back,.overshoot=1.45};
    friend bool operator==(const CheckBoxConfiguration&,const CheckBoxConfiguration&) = default;
};
struct CheckBoxInteraction final {
    bool enabled=true,hovered=false,pressed=false,focused=false;
    friend bool operator==(const CheckBoxInteraction&,const CheckBoxInteraction&)=default;
};
struct CheckBoxVisual final {
    StyleColor fill,mark,border,text,focus;
    double border_width=0;
    bool draw_mark=false,focused=false,figma_mark=false,draw_inner_shadow=false;
};
struct CheckBoxRect final { double x=0,y=0,width=0,height=0; };
struct CheckBoxGeometry final {
    CheckBoxRect box,text;
    double radius=0;
    bool draw_box=false,draw_text=false;
};


} // namespace lvui
