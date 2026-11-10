#pragma once
// Generated from the LVUI Figma component snapshot; do not edit.
#include <LVUI/StyleTokens.h>
namespace lvui::search_field_material {
struct SearchGradientStop final { double position; StyleColor color; };
struct SearchShadow final { double radius, offset_x, offset_y; StyleColor color; };
inline constexpr double blur_radius = 8.0;
inline constexpr double inline_blur_radius = 6.0;
inline constexpr SearchGradientStop fill_gradient[] = {
    {0.0, {0.0, 0.0, 0.0, 0.20000000298023224}},
    {0.5199999809265137, {0.0, 0.0, 0.0, 0.0}},
    {1.0, {1.0, 1.0, 1.0, 0.03500000014901161}},
};
inline constexpr SearchGradientStop stroke_gradient[] = {
    {0.0, {0.0, 0.0, 0.0, 0.2800000011920929}},
    {0.5, {1.0, 1.0, 1.0, 0.014999999664723873}},
    {1.0, {1.0, 1.0, 1.0, 0.17000000178813934}},
};
inline constexpr SearchGradientStop disabled_fill_gradient[] = {
    {0.0, {0.0, 0.0, 0.0, 0.04500000178813934}},
    {0.5199999809265137, {0.0, 0.0, 0.0, 0.0}},
    {1.0, {1.0, 1.0, 1.0, 0.014999999664723873}},
};
inline constexpr SearchGradientStop inline_fill_gradient[] = {
    {0.0, {0.0, 0.0, 0.0, 0.05999999865889549}},
    {0.5199999809265137, {0.0, 0.0, 0.0, 0.0}},
    {1.0, {1.0, 1.0, 1.0, 0.014999999664723873}},
};
inline constexpr SearchShadow inner_shadows[] = {
    {2.0, 0.5, 1.2000000476837158, {0.0, 0.0, 0.0, 0.30000001192092896}},
    {0.800000011920929, 0.0, -0.5, {1.0, 1.0, 1.0, 0.1599999964237213}},
};
inline constexpr SearchShadow inline_inner_shadows[] = {
    {1.0, 0.0, 0.6499999761581421, {0.0, 0.0, 0.0, 0.11999999731779099}},
    {0.6000000238418579, 0.0, -0.5, {1.0, 1.0, 1.0, 0.054999999701976776}},
};
} // namespace lvui::search_field_material
