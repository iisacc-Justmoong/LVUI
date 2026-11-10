#pragma once
#include <array>
// Generated from the LVUI Figma radial material snapshot; do not edit.
namespace lvui::material_radial_defaults {
struct Field final { double x_factor,x_offset,y_factor,y_offset,radius_x,radius_y; bool intense; };
inline constexpr double first_position = 0.38;
inline constexpr double second_position = 0.72;
inline constexpr double first_opacity = 0.55;
inline constexpr double second_opacity = 0.13;
inline constexpr std::array<double,3> edge_rgb{{0.08120492845773697, 0.5099760293960571, 1.0}};
inline constexpr std::array<Field,8> fields{{
    {0.5, -129.0, 0.5, -28.0, 200.0, 200.0, true},
    {0.5, 136.0, 0.5, 62.0, 180.0, 180.0, false},
    {0.0, 48.0, 0.0, 32.0, 200.0, 200.0, true},
    {1.0, -64.0, 0.0, 40.0, 260.0, 150.0, false},
    {0.0, 0.0, 0.5, 0.0, 140.0, 300.0, false},
    {1.0, 0.0, 0.5, 0.0, 160.0, 300.0, true},
    {0.5, 0.0, 1.0, 0.0, 380.0, 150.0, false},
    {0.0, 72.0, 1.0, -40.0, 180.0, 180.0, true},
}};
} // namespace lvui::material_radial_defaults
