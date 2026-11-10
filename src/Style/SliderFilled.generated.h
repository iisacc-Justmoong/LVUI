#pragma once
// Generated from the LVUI Figma component snapshot; do not edit.
#include <array>
namespace lvui::slider_filled_defaults {
struct SizePreset final { double height, track_height, thumb_diameter, range_inset; bool show_symbol; };
inline constexpr std::array<SizePreset, 4> sizes{{
    SizePreset{22.0, 12.0, 8.0, 6.0, false},
    SizePreset{24.0, 16.0, 8.0, 8.0, false},
    SizePreset{32.0, 28.0, 20.0, 14.0, true},
    SizePreset{44.0, 44.0, 36.0, 22.0, true},
}};
inline constexpr double ring_outset = 3.0;
inline constexpr double hover_ring_stroke = 1.0;
inline constexpr double pressed_ring_stroke = 2.0;
inline constexpr double blur_radius = 6.0;
inline constexpr double highlight_opacity = 0.054999999701976776;
inline constexpr double highlight_offset_y = -0.5;
} // namespace lvui::slider_filled_defaults
