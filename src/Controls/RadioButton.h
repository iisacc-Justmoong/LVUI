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
struct RadioButtonPalette final {
    StyleColor on{figma_defaults::accent_blue_default},off{1,1,1,.9};
    StyleColor disabled_on{figma_defaults::panel_surface_raised};
    StyleColor disabled_off{figma_defaults::panel_surface_raised};
    StyleColor dot{1,1,1,.9},disabled_dot{1,1,1,.5};
    StyleColor text{1,1,1,.8},disabled_text{1,1,1,.3},focus{figma_defaults::accent_blue_default};
    friend bool operator==(const RadioButtonPalette&,const RadioButtonPalette&) = default;
};
struct RadioButtonConfiguration final {
    std::string text,accessibility_name,group;
    bool enabled=true,checked=false,motion_enabled=true;
    double indicator_size=18,dot_size=8,label_gap=8,font_size=13,line_height=13;
    unsigned font_weight=500;
    Motion motion{.duration=std::chrono::milliseconds{360},.curve=MotionCurve::out_back,.overshoot=1.45};
    RadioButtonPalette palette;
    friend bool operator==(const RadioButtonConfiguration&,const RadioButtonConfiguration&) = default;
};
struct RadioButtonInteraction final {bool enabled=true,focused=false;};
struct RadioButtonVisual final {StyleColor indicator,dot,text,focus;bool draw_dot=false,focused=false;};
struct RadioButtonRect final {double x=0,y=0,width=0,height=0;};
struct RadioButtonGeometry final {RadioButtonRect indicator,dot,text;double indicator_radius=0,dot_radius=0;bool draw_indicator=false,draw_dot=false,draw_text=false;};


} // namespace lvui
