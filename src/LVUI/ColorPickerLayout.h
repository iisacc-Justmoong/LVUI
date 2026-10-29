#pragma once

#include "ColorChannels.h"
#include "ColorPicker.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace lvui {

struct ColorPickerRect final {
    double x = 0, y = 0, width = 0, height = 0;
    friend bool operator==(const ColorPickerRect&, const ColorPickerRect&) = default;
};

struct ColorPickerLayout final {
    double width = 0, height = 0;
    ColorSurfaceKind surface_kind = ColorSurfaceKind::wheel;
    std::optional<ColorSurfaceKind> rail_kind;
    ColorPickerRect surface, rail, divider, comparison, hex_label, hex_field;
    ColorPickerRect alpha_label, alpha_field, alpha_suffix, recent_label;
    ColorPickerRect eyedropper, color_space, cancel, apply;
    std::vector<std::pair<ColorChannel, ColorPickerRect>> channels;
    std::vector<ColorPickerRect> recent;
};

// Pure geometry: all dimensions are logical units, independent of the host
// widget tree. Empty rectangles denote controls that are not visible.
[[nodiscard]] inline std::optional<ColorPickerLayout> color_picker_layout(const ColorPickerConfiguration& input) {
    if (!ColorPicker::validate(input)) return std::nullopt;
    ColorPickerLayout result; result.width = input.width;
    const auto x = input.padding, inner = input.width - input.padding * 2;
    double y = input.padding; bool first = true;
    const auto section = [&](double height) {
        if (!first) y += 12;
        first = false; ColorPickerRect bounds{x, y, inner, height}; y += height; return bounds;
    };
    std::vector<ColorChannel> before;
    switch (input.mode) {
    case ColorPickerMode::wheel: before = {ColorChannel::hue, ColorChannel::saturation, ColorChannel::brightness}; break;
    case ColorPickerMode::grayscale: before = {ColorChannel::gray}; break;
    case ColorPickerMode::rgb: before = {ColorChannel::red, ColorChannel::green, ColorChannel::blue}; break;
    case ColorPickerMode::cmyk: before = {ColorChannel::cyan, ColorChannel::magenta, ColorChannel::yellow, ColorChannel::black}; break;
    case ColorPickerMode::hue_saturation: case ColorPickerMode::saturation_brightness: break;
    }
    if (!before.empty()) {
        const auto group = section(before.size() * 22. + (before.size() - 1) * 4.);
        for (std::size_t i = 0; i < before.size(); ++i)
            result.channels.push_back({before[i], {x, group.y + i * 26., inner, 22}});
    }
    const bool plane = input.mode == ColorPickerMode::hue_saturation || input.mode == ColorPickerMode::saturation_brightness;
    const auto domain = section(plane ? inner - 36 : input.mode == ColorPickerMode::wheel ? std::min(inner, 232.) : 184.);
    result.surface = domain;
    if (plane) {
        result.surface.width = inner - 36;
        result.rail = {x + inner - 24, domain.y, 24, domain.height};
        if (input.mode == ColorPickerMode::hue_saturation) {
            result.surface_kind = ColorSurfaceKind::hue_saturation;
            result.rail_kind = ColorSurfaceKind::brightness_rail;
        } else {
            result.surface_kind = ColorSurfaceKind::saturation_brightness;
            result.rail_kind = ColorSurfaceKind::hue_rail;
        }
        const auto row = section(22);
        result.channels.push_back({input.mode == ColorPickerMode::hue_saturation ? ColorChannel::brightness : ColorChannel::hue, row});
    } else if (input.mode != ColorPickerMode::wheel) result.surface_kind = ColorSurfaceKind::spectrum;
    result.divider = section(1);
    const auto values = section(28);
    result.comparison = {x, values.y, 40, 28};
    result.hex_label = {x + 48, values.y, 25, 28};
    result.hex_field = {x + 81, values.y + 3, std::max(40., inner - 187), 22};
    result.alpha_label = {x + inner - 98, values.y, 10, 28};
    result.alpha_field = {x + inner - 80, values.y + 3, 56, 22};
    result.alpha_suffix = {x + inner - 16, values.y, 16, 28};
    if (input.show_recent_colors) {
        const auto recent = section(20);
        result.recent_label = {x, recent.y, 40, 20};
        const auto count = input.recent_colors.size();
        if (count) {
            const auto width = std::max(1., (inner - 48 - (count - 1) * 8.) / count);
            for (std::size_t i = 0; i < count; ++i)
                result.recent.push_back({x + 48 + i * (width + 8), recent.y, width, 20});
        }
    }
    if (input.show_actions) {
        const auto actions = section(22);
        result.eyedropper = {x, actions.y, 22, 22};
        result.apply = {x + inner - 68, actions.y, 68, 22};
        result.cancel = {result.apply.x - 76, actions.y, 68, 22};
        result.color_space = {x + 30, actions.y, std::max(0., result.cancel.x - x - 38), 22};
    }
    result.height = y + input.padding;
    return result;
}

} // namespace lvui
