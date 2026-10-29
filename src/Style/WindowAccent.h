#pragma once

#include <LVUI/StyleTokens.h>
#include <array>
#include <optional>
#include <string_view>

namespace lvui {

// Accent selections belong to LVUI and do not require an application UI toolkit.
enum class WindowAccent { blue, purple, rose, red, orange, yellow, green, gray, brown, cyan, teal, white };

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
        {.08120492845773697, .5099760293960571, 1, 1},
        {.5275065302848816, .291259765625, 1, 1},
        {.975510835647583, .26653972268104553, .4437825083732605, 1},
        {1, .2760854959487915, .2760854959487915, 1},
        {1, .47672274708747864, .15144230425357819, 1},
        {1, .8303598165512085, .15179912745952606, 1},
        {0, .9058823585510254, .12156862765550613, 1},
        {140.0 / 255, 140.0 / 255, 140.0 / 255, 1},
        {.2616060674190521, .13824503123760223, .09279626607894897, 1},
        {.12926307320594788, .8258525729179382, 1, 1},
        {0, .7790151238441467, .8655723929405212, 1},
        {1, 1, 1, 1},
    }};
    if (!valid_window_accent(value)) return std::nullopt;
    return colors[static_cast<std::size_t>(value)];
}

} // namespace lvui
