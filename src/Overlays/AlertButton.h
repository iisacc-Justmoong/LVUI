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
    StyleColor disabled_background{.14820532500743866, .15256911516189575, .15256911516189575, 1};
    StyleColor normal_pressed{.1555851548910141, .15707255899906158, .15855994820594788, 1};
    friend bool operator==(const AlertButtonConfiguration&, const AlertButtonConfiguration&) = default;
};

struct AlertButtonAppearance final {
    ButtonConfiguration button;
    std::optional<StyleColor> border;
    double border_width = 0;
};

// AlertButton shares LVUI's button interaction and text layout while owning
// its compact/dialog metrics and outlined alert presentation.
class AlertButton final {
public:
    [[nodiscard]] static std::optional<AlertButtonAppearance> resolve(const AlertButtonConfiguration& value) {
        if ((value.text_color && !valid_color_value(*value.text_color)) ||
            (value.primary_color && !valid_color_value(*value.primary_color)) ||
            !valid_color_value(value.dialog_text) || !valid_color_value(value.dialog_border) ||
            !valid_color_value(value.disabled_background) || !valid_color_value(value.normal_pressed)) return std::nullopt;
        AlertButtonAppearance result;
        auto& button = result.button;
        button.text = value.text;
        button.accessibility_name = value.accessibility_name.empty() ? value.text : value.accessibility_name;
        button.enabled = value.enabled;
        button.tone = value.tone;
        button.width = value.width;
        button.height = value.height;
        button.horizontal_padding = value.horizontal_padding.value_or(value.dialog_style ? 16 : 8);
        button.corner_radius = value.corner_radius.value_or(value.dialog_style ? 16 : 4);
        button.palette = value.palette;
        button.palette.disabled_background = value.disabled_background;
        if (value.primary_color) button.palette.primary = *value.primary_color;
        const auto darken = [](StyleColor color, double factor) {
            return StyleColor{color.red / factor, color.green / factor, color.blue / factor, color.alpha};
        };
        button.palette.primary_hover = darken(button.palette.primary, 1.12);
        button.palette.primary_pressed = darken(button.palette.primary, 1.2);
        if (value.dialog_style) {
            button.palette.text = value.dialog_text;
            button.palette.normal = {0, 0, 0, 0};
            button.palette.normal_hover = {1, 1, 1, .05};
            button.palette.normal_pressed = {1, 1, 1, .09};
            if (value.tone == ButtonTone::normal) {
                result.border = value.dialog_border;
                result.border_width = 1;
                button.border_color = result.border;
                button.border_width = result.border_width;
            }
        } else {
            const auto lighter = [](double channel) { return std::min(1., channel * 1.08); };
            const auto normal = button.palette.normal;
            button.palette.normal_hover = {lighter(normal.red), lighter(normal.green), lighter(normal.blue), normal.alpha};
            button.palette.normal_pressed = value.normal_pressed;
        }
        if (value.text_color) button.palette.text = *value.text_color;
        if (value.tone == ButtonTone::borderless && (value.dialog_style || value.text_color)) {
            button.palette.primary = value.text_color.value_or(value.dialog_text);
        }
        if (!Button::validate(button)) return std::nullopt;
        return result;
    }

    [[nodiscard]] const AlertButtonConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] const AlertButtonAppearance& appearance() const noexcept { return appearance_; }

    bool set_configuration(AlertButtonConfiguration value) {
        auto appearance = resolve(value);
        if (!appearance) return false;
        configuration_ = std::move(value);
        appearance_ = std::move(*appearance);
        return true;
    }

private:
    AlertButtonConfiguration configuration_;
    AlertButtonAppearance appearance_ = *resolve(configuration_);
};

} // namespace lvui
