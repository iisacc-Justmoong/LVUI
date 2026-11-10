#pragma once

#include <LVUI/ColorSource.h>
#include <LVUI/Text.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace lvui {

enum class ModalShape { rounded, cylinder };

struct ModalPalette final {
    StyleColor frame{figma_defaults::panel_surface_panel};
    StyleColor backdrop{0, 0, 0, 89. / 255};
    StyleColor icon_background{figma_defaults::panel_surface_interactive};
    StyleColor icon_border{208. / 255, 215. / 255, 226. / 255, .18};
    friend bool operator==(const ModalPalette&, const ModalPalette&) = default;
};

struct ModalConfiguration final {
    std::string title, description, message, icon_name;
    std::string accessibility_name = "Modal";
    std::array<std::string, 3> action_text{"OK", "", ""};
    std::array<bool, 3> action_enabled{true, true, true};
    unsigned action_count = 0; // Zero derives the count from nonempty action text.
    bool show_icon = true, dismiss_on_background = true, dismiss_on_escape = true, motion_enabled = true;
    double minimum_width = 520, maximum_width = 980, preferred_width = 760;
    double side_padding = 24, minimum_height = 210, vertical_offset = -16;
    double content_padding = 20, section_spacing = 20, action_spacing = 12;
    double corner_radius = 12, icon_size = 72, icon_corner_radius = 16;
    double enter_duration_ms = 420, exit_duration_ms = 150, overshoot = 1.45, initial_scale = .92;
    ModalShape shape = ModalShape::rounded;
    ModalPalette palette;
    friend bool operator==(const ModalConfiguration&, const ModalConfiguration&) = default;
};

struct ModalRect final {
    double x = 0, y = 0, width = 0, height = 0;
    friend bool operator==(const ModalRect&, const ModalRect&) = default;
};

struct ModalAction final {
    std::string_view text;
    bool visible = false, enabled = false;
};

} // namespace lvui
