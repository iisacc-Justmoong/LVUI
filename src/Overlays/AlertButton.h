#pragma once

#include <LVUI/Button.h>
#include <LVUI/ColorSource.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {

struct AlertButtonConfiguration final {
    std::string text = "Button", accessibility_name;
    ButtonTone tone = ButtonTone::normal;
    bool enabled = true, dialog_style = false;
    double height = 20;
    std::optional<double> width, horizontal_padding, corner_radius;
    std::optional<StyleColor> text_color, primary_color;
    ButtonPalette palette;
    StyleColor dialog_text{244. / 255, 245. / 255, 247. / 255, 1};
    StyleColor dialog_border{89. / 255, 97. / 255, 104. / 255, 1};
    StyleColor disabled_background{figma_defaults::panel_surface_control};
    StyleColor normal_pressed{figma_defaults::panel_surface_input};
    bool motion_enabled = true;
    bool enter_key_activation = false;
    friend bool operator==(const AlertButtonConfiguration&, const AlertButtonConfiguration&) = default;
};

struct AlertButtonAppearance final {
    ButtonConfiguration button;
    std::optional<StyleColor> border;
    double border_width = 0;
};

} // namespace lvui
