#pragma once
// Figma LVUI TabBar/LVRS Mobile, 1074:1946 and 1074:2005.
// Space/16, Space/12, Space/4, Radius/28, Radius/24, Radius/12 and Radius/8.
namespace lvui::mobile_navigation_tokens {
inline constexpr double outer=16,gap=12,inset=4,group_height=56,tab_width=56,touch_size=48;
inline constexpr double ios_radius=28,ios_selected_radius=24,android_radius=12,android_selected_radius=8;
inline constexpr double icon_size=24,label_size=11,label_line_height=11,icon_label_gap=2,border_width=1;
// Badge and transition metrics from the mobile navigation component contract.
inline constexpr double badge_dot=6,badge_height=16,badge_padding=4,badge_offset=2;
}
