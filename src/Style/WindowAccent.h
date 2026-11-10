#pragma once

#include <LVUI/StyleTokens.h>
#include <array>
#include <cmath>
#include <optional>
#include <string_view>

namespace lvui {

// Accent selections belong to LVUI and do not require an application UI toolkit.
enum class WindowAccent { blue, purple, rose, red, orange, yellow, green, gray, brown, cyan, teal, white };

// A resolved palette can be passed to a composite without coupling it to a
// window or to the lifetime of a reactive binding.
struct WindowAccentPalette final {
    StyleColor primary{figma_defaults::accent_blue_default};
    StyleColor hover{figma_defaults::accent_blue_hover};
    StyleColor pressed{figma_defaults::accent_blue_pressed};
    StyleColor subtle{figma_defaults::accent_blue_subtle};
    StyleColor selected{figma_defaults::accent_blue_selected};
    friend bool operator==(const WindowAccentPalette&, const WindowAccentPalette&) = default;
};

[[nodiscard]] inline bool valid_window_accent_palette(const WindowAccentPalette& value) noexcept {
    const auto valid = [](StyleColor color) {
        const auto channel = [](double c) { return std::isfinite(c) && c >= 0 && c <= 1; };
        return channel(color.red) && channel(color.green) && channel(color.blue) && channel(color.alpha);
    };
    return valid(value.primary) && valid(value.hover) && valid(value.pressed) &&
           valid(value.subtle) && valid(value.selected);
}

inline constexpr std::array<std::string_view, 12> window_accent_names{
    "blue", "purple", "rose", "red", "orange", "yellow", "green", "gray", "brown", "cyan", "teal", "white"};

[[nodiscard]] constexpr bool valid_window_accent(WindowAccent value) noexcept {
    return static_cast<unsigned long long>(value) < window_accent_names.size();
}

[[nodiscard]] constexpr std::optional<WindowAccent> parse_window_accent(std::string_view name) noexcept {
    for (std::size_t i = 0; i < window_accent_names.size(); ++i)
        if (window_accent_names[i] == name) return static_cast<WindowAccent>(i);
    return std::nullopt;
}

// Design snapshot fallback for accents without a corresponding style token.
[[nodiscard]] constexpr std::optional<StyleColor> fallback_window_accent(WindowAccent value) noexcept {
    constexpr std::array<StyleColor, 12> colors{{
        {figma_defaults::accent_blue_default},
        {figma_defaults::accent_purple_default},
        {figma_defaults::accent_rose_default},
        {figma_defaults::accent_red_default},
        {figma_defaults::accent_orange_default},
        {figma_defaults::accent_yellow_default},
        {figma_defaults::accent_green_default},
        {140.0 / 255, 140.0 / 255, 140.0 / 255, 1},
        {figma_defaults::accent_brown_default},
        {figma_defaults::accent_cyan_default},
        {figma_defaults::accent_teal_default},
        {1, 1, 1, 1},
    }};
    if (!valid_window_accent(value)) return std::nullopt;
    return colors[static_cast<std::size_t>(value)];
}

} // namespace lvui
