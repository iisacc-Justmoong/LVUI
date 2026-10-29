#pragma once

#include <LVUI/Button.h>
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

enum class AlertShape { rounded, cylinder };

struct AlertPalette final {
    StyleColor frame{.1370945126, .1402995437, .1419020444, .85};
    StyleColor backdrop{0, 0, 0, .4};
    StyleColor border{1, 1, 1, .2};
    StyleColor title{1, 1, 1, .9};
    StyleColor description{1, 1, 1, .8};
    friend bool operator==(const AlertPalette&, const AlertPalette&) = default;
};

struct AlertConfiguration final {
    std::string title, description, message, accessibility_name = "Alert";
    std::array<std::string, 3> action_text;
    std::array<bool, 3> action_enabled{true, true, true};
    unsigned action_count = 0; // Zero selects a count from the supplied labels.
    std::optional<bool> secondary_destructive;
    std::optional<StyleColor> primary_color;
    bool show_icon = true, dismiss_on_background = false, dismiss_on_escape = false;
    bool motion_enabled = true, glass_enabled = false, shadow_enabled = false;
    double minimum_width = 280, maximum_width = 500, preferred_width = 500, side_padding = 24;
    double content_top = 46, content_bottom = 36, text_padding = 24, icon_spacing = 28, text_spacing = 14;
    double icon_extent = 86, corner_radius = 36, border_width = 1, blur_radius = 28;
    double enter_duration_ms = 420, exit_duration_ms = 150, overshoot = 1.45, initial_scale = .92;
    AlertShape shape = AlertShape::rounded;
    AlertPalette palette;
    friend bool operator==(const AlertConfiguration&, const AlertConfiguration&) = default;
};

struct AlertRect final {
    double x = 0, y = 0, width = 0, height = 0;
    friend bool operator==(const AlertRect&, const AlertRect&) = default;
};

struct AlertAction final {
    std::string_view text;
    bool visible = false, enabled = false, destructive = false;
    ButtonTone tone = ButtonTone::normal;
};

// Child rectangles are local to the centered frame. Text heights are measured
// by the host using the width and fonts selected by the application adapter.
struct AlertLayout final {
    AlertRect frame, icon, title, description;
    std::array<AlertRect, 3> actions;
    double radius = 0;
};

class Alert final {
public:
    [[nodiscard]] static bool validate(const AlertConfiguration& value) noexcept {
        const auto range = [](double n, double low, double high) {
            return std::isfinite(n) && n >= low && n <= high;
        };
        if (!valid_accessibility_text(value.title) || !valid_accessibility_text(value.description) ||
            !valid_accessibility_text(value.message) || !valid_accessibility_text(value.accessibility_name)) return false;
        for (const auto& text : value.action_text) if (!valid_accessibility_text(text)) return false;
        if (value.action_count > 3 || !range(value.minimum_width, 0, 8192) ||
            !range(value.maximum_width, 1, 8192) || value.minimum_width > value.maximum_width ||
            !range(value.preferred_width, 1, 8192) || !range(value.side_padding, 0, 8192) ||
            !range(value.content_top, 0, 8192) || !range(value.content_bottom, 0, 8192) ||
            !range(value.text_padding, 0, 8192) || !range(value.icon_spacing, 0, 8192) ||
            !range(value.text_spacing, 0, 8192) || !range(value.icon_extent, 1, 256) ||
            !range(value.corner_radius, 0, 8192) || !range(value.border_width, 0, 8192) ||
            !range(value.blur_radius, 0, 64) || !range(value.enter_duration_ms, 0, 86400000) ||
            !range(value.exit_duration_ms, 0, 86400000) || !range(value.overshoot, 0, 4) ||
            !range(value.initial_scale, .5, 1)) return false;
        if (value.shape != AlertShape::rounded && value.shape != AlertShape::cylinder) return false;
        return (!value.primary_color || valid_color_value(*value.primary_color)) &&
               valid_color_value(value.palette.frame) && valid_color_value(value.palette.backdrop) &&
               valid_color_value(value.palette.border) && valid_color_value(value.palette.title) &&
               valid_color_value(value.palette.description);
    }

    bool set_configuration(AlertConfiguration value) {
        if (!validate(value)) return false;
        configuration_ = std::move(value);
        return true;
    }

    [[nodiscard]] const AlertConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] std::string_view description() const noexcept {
        return configuration_.description.empty() ? configuration_.message : configuration_.description;
    }

    [[nodiscard]] unsigned action_count() const noexcept {
        if (configuration_.action_count) return configuration_.action_count;
        for (unsigned index = 3; index > 1; --index) {
            if (!configuration_.action_text[index - 1].empty()) return index;
        }
        return 1;
    }

    [[nodiscard]] AlertAction action(unsigned index) const noexcept {
        if (index >= action_count()) return {};
        const auto& text = configuration_.action_text[index];
        const bool destructive = index == 1 && configuration_.secondary_destructive.value_or(action_count() == 3);
        const auto tone = index == 0 ? ButtonTone::primary : ButtonTone::normal;
        return {text.empty() ? std::string_view{"Button"} : std::string_view{text},
                true, configuration_.action_enabled[index], destructive, tone};
    }

    [[nodiscard]] std::optional<AlertLayout> layout(double viewport_width, double viewport_height,
                                                     double title_height, double description_height) const noexcept {
        const auto extent = [](double n) { return std::isfinite(n) && n >= 0 && n <= 8192; };
        if (!extent(viewport_width) || !extent(viewport_height) || !extent(title_height) || !extent(description_height)) {
            return std::nullopt;
        }
        const auto& c = configuration_;
        const auto width = std::max(0., std::min({viewport_width - 2 * c.side_padding, c.maximum_width,
                                                  std::max(c.minimum_width, c.preferred_width)}));
        auto result = layout_for_width(width, title_height, description_height);
        if (!result) return std::nullopt;
        result->frame.x = (viewport_width - width) / 2;
        result->frame.y = (viewport_height - result->frame.height) / 2;
        return result;
    }

    // Use an already allocated popup width without assuming its host viewport.
    [[nodiscard]] std::optional<AlertLayout> layout_for_width(double width, double title_height,
                                                               double description_height) const noexcept {
        const auto extent = [](double n) { return std::isfinite(n) && n >= 0 && n <= 8192; };
        if (!extent(width) || !extent(title_height) || !extent(description_height)) return std::nullopt;
        const auto& c = configuration_;
        const auto text_width = std::max(0., width - 2 * c.text_padding);
        AlertLayout result;
        auto y = c.content_top;
        if (c.show_icon) {
            const auto icon_width = std::min(width, c.icon_extent);
            result.icon = {(width - icon_width) / 2, y, icon_width, c.icon_extent};
            y += c.icon_extent;
            if (!c.title.empty() || !description().empty()) y += c.icon_spacing;
        }
        if (!c.title.empty()) {
            result.title = {std::min(c.text_padding, width / 2), y, text_width, title_height};
            y += title_height;
        }
        if (!description().empty()) {
            if (!c.title.empty()) y += c.text_spacing;
            result.description = {std::min(c.text_padding, width / 2), y, text_width, description_height};
            y += description_height;
        }
        y += c.content_bottom;
        const auto action_width = std::max(0., width - 48);
        const auto action_x = std::min(24., width / 2);
        if (action_count() == 3) {
            y += 18;
            result.actions[0] = {action_x, y, action_width, 56}; y += 68;
            result.actions[1] = {action_x, y, action_width, 56}; y += 68;
            result.actions[2] = {action_x, y, action_width, 44}; y += 44 + 18;
        } else {
            y += 28;
            if (action_count() == 2) {
                const auto gap = std::min(14., action_width);
                const auto half = (action_width - gap) / 2;
                result.actions[1] = {action_x, y, half, 56};
                result.actions[0] = {action_x + half + gap, y, half, 56};
            } else {
                result.actions[0] = {action_x, y, action_width, 56};
            }
            y += 56 + 32;
        }
        y += c.border_width;
        if (!extent(width) || !extent(y)) return std::nullopt;
        result.frame = {0, 0, width, y};
        const auto short_edge = std::min(width, y);
        result.radius = c.shape == AlertShape::cylinder ? short_edge / 2 : std::min(c.corner_radius, short_edge * .4);
        return result;
    }

private:
    AlertConfiguration configuration_;
};

} // namespace lvui
