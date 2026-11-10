#pragma once
#include <LVUI/Motion.h>
#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
struct ToggleSwitchPalette final {
    StyleColor on{figma_defaults::accent_blue_default},on_hover{figma_defaults::accent_blue_hover},on_pressed{figma_defaults::accent_blue_pressed};
    StyleColor off{figma_defaults::panel_surface_raised},off_hover{figma_defaults::panel_surface_alt},off_pressed{figma_defaults::accent_blue_subtle};
    StyleColor disabled{figma_defaults::panel_surface_raised},knob{1,1,1,.9},disabled_knob{1,1,1,.5};
    StyleColor text{1,1,1,.8},disabled_text{1,1,1,.3},focus{figma_defaults::accent_blue_default};
    friend bool operator==(const ToggleSwitchPalette&,const ToggleSwitchPalette&) = default;
};
struct ToggleSwitchConfiguration final {
    std::string text,accessibility_name;
    bool enabled=true,checked=false,right_to_left=false,motion_enabled=true;
    double track_width=38,track_height=22,track_padding=2,knob_size=18,track_radius=20,knob_radius=9;
    double label_gap=8,font_size=13,line_height=13;
    unsigned font_weight=500;
    Motion motion{.duration=std::chrono::milliseconds{320},.curve=MotionCurve::out_back,.overshoot=1.45};
    ToggleSwitchPalette palette;
    friend bool operator==(const ToggleSwitchConfiguration&,const ToggleSwitchConfiguration&) = default;
};
struct ToggleSwitchInteraction final { bool enabled=true,hovered=false,pressed=false,focused=false; };
struct ToggleSwitchVisual final { StyleColor track,knob,text,focus; bool focused=false; };
struct ToggleSwitchRect final { double x=0,y=0,width=0,height=0; };
struct ToggleSwitchGeometry final {
    ToggleSwitchRect track,knob,text;
    double track_radius=0,knob_radius=0;
    bool draw_track=false,draw_knob=false,draw_text=false;
};

} // namespace lvui
