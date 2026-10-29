#pragma once

#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace lvui {

struct HierarchyItemPalette final {
    StyleColor idle{0, 0, 0, 0}, hover{1, 1, 1, .06}, pressed{1, 1, 1, .12};
    StyleColor active{.0812049285, .5099760294, 1, .24}, inactive{1, 1, 1, .12}, drag{.0812049285, .5099760294, 1, .16};
    StyleColor text{1, 1, 1, .9}, disabled_text{1, 1, 1, .35}, focus{.0812049285, .5099760294, 1, 1};
    friend bool operator==(const HierarchyItemPalette&, const HierarchyItemPalette&) = default;
};
struct HierarchyItemConfiguration final {
    std::string key, label, icon_name, accessibility_name;
    std::optional<std::int64_t> id;
    std::int64_t count = -1;
    std::size_t depth = 0;
    double width = 200, height = 20, indent_step = 8, icon_size = 18, chevron_size = 16;
    double left_padding = 8, right_padding = 8, leading_spacing = 2, count_width = 24, corner_radius = 5;
    bool enabled = true, activatable = true, selected = false, expanded = false, show_chevron = true;
    bool has_children = false, drag_preview = false;
    HierarchyItemPalette palette;
    friend bool operator==(const HierarchyItemConfiguration&, const HierarchyItemConfiguration&) = default;
};
struct HierarchyItemRect final {
    double x = 0, y = 0, width = 0, height = 0;
    friend bool operator==(const HierarchyItemRect&, const HierarchyItemRect&) = default;
};
struct HierarchyItemGeometry final {
    HierarchyItemRect surface, icon, label, count, chevron;
    bool show_icon = false, show_count = false, show_chevron = false;
};
enum class HierarchyItemPhase { idle, hover, pressed, active, inactive, drag };
struct HierarchyItemInteraction final { bool hovered = false, pressed = false, focused = false; };
struct HierarchyItemVisual final {
    HierarchyItemPhase phase = HierarchyItemPhase::idle;
    StyleColor background, text, focus;
    bool focused = false;
};

// LVUI owns row metrics, state precedence and expansion policy; the app owns
// structural edits and can replace the backing tree independently.
class HierarchyItem final {
public:
    [[nodiscard]] static bool validate(const HierarchyItemConfiguration& value) noexcept {
        const auto bound = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
        const auto color = [&](StyleColor c) { return bound(c.red, 0, 1) && bound(c.green, 0, 1) && bound(c.blue, 0, 1) && bound(c.alpha, 0, 1); };
        const auto& p = value.palette;
        return valid_accessibility_text(value.key) && value.key.size() <= 1024 && valid_accessibility_text(value.label) &&
            valid_accessibility_text(value.icon_name) && valid_accessibility_text(value.accessibility_name) && value.count >= -1 &&
            value.depth <= 65536 && bound(value.width, 1, 8192) && bound(value.height, 1, 8192) &&
            bound(value.indent_step, 0, 8192) && bound(value.icon_size, 0, 256) && bound(value.chevron_size, 0, 256) &&
            bound(value.left_padding, 0, 8192) && bound(value.right_padding, 0, 8192) &&
            bound(value.leading_spacing, 0, 8192) && bound(value.count_width, 0, 8192) && bound(value.corner_radius, 0, 8192) &&
            color(p.idle) && color(p.hover) && color(p.pressed) && color(p.active) && color(p.inactive) && color(p.drag) &&
            color(p.text) && color(p.disabled_text) && color(p.focus);
    }
    bool set_configuration(HierarchyItemConfiguration value) {
        if (!validate(value)) return false; configuration_ = std::move(value); return true;
    }
    [[nodiscard]] const HierarchyItemConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] std::optional<HierarchyItemGeometry> geometry(double width, double height) const noexcept {
        if (!std::isfinite(width) || !std::isfinite(height) || width < 0 || height < 0 || width > 8192 || height > 8192) return std::nullopt;
        const auto& c = configuration_; HierarchyItemGeometry result;
        result.surface = {0, 0, width, height};
        result.show_icon = !c.icon_name.empty() && c.icon_size > 0;
        result.show_count = c.count >= 0 && c.count_width > 0;
        result.show_chevron = c.show_chevron && c.has_children && c.chevron_size > 0;
        auto left = std::clamp(c.left_padding + double(c.depth) * c.indent_step, 0., width);
        if (result.show_icon) {
            const auto size = std::min({c.icon_size, height, std::max(0., width - left)});
            result.icon = {left, (height - size) / 2, size, size}; left = std::min(width, left + size + c.leading_spacing);
        }
        auto right = std::clamp(width - c.right_padding, 0., width);
        if (result.show_chevron || result.show_count) {
            const auto size = std::min({c.chevron_size, height, right});
            const auto anchor = std::max(0., right - size);
            if (result.show_chevron) result.chevron = {anchor, (height - size) / 2, size, size};
            right = anchor;
        }
        if (result.show_count) {
            const auto end = std::max(0., right - 8);
            const auto size = std::min(c.count_width, end);
            result.count = {end - size, 0, size, height}; right = end - size;
        }
        result.label = {left, 0, std::max(0., right - left), height};
        return result;
    }
    [[nodiscard]] HierarchyItemVisual visual(HierarchyItemInteraction interaction) const noexcept {
        const auto& c = configuration_; const auto& p = c.palette;
        const auto phase = c.drag_preview ? HierarchyItemPhase::drag : !c.enabled ? HierarchyItemPhase::inactive :
            c.selected ? HierarchyItemPhase::active : interaction.pressed ? HierarchyItemPhase::pressed :
            interaction.hovered ? HierarchyItemPhase::hover : HierarchyItemPhase::idle;
        const auto background = phase == HierarchyItemPhase::drag ? p.drag : phase == HierarchyItemPhase::inactive ? p.inactive :
            phase == HierarchyItemPhase::active ? p.active : phase == HierarchyItemPhase::pressed ? p.pressed :
            phase == HierarchyItemPhase::hover ? p.hover : p.idle;
        return {phase, background, c.enabled ? p.text : p.disabled_text, p.focus, c.enabled && interaction.focused};
    }
    [[nodiscard]] std::optional<bool> toggle_expansion() noexcept {
        if (!configuration_.enabled || !configuration_.show_chevron || !configuration_.has_children || !configuration_.chevron_size) return std::nullopt;
        configuration_.expanded = !configuration_.expanded; return configuration_.expanded;
    }
private:
    HierarchyItemConfiguration configuration_;
};

} // namespace lvui
