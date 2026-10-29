#pragma once

#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace lvui {

enum class ListItemVariant { mini, detail, navigation, toggle, checkable, action, action_group, stepper, select, inline_edit };
using ListItemValue = std::variant<bool, std::int64_t, double, std::string>;
struct ListItemPalette final {
    StyleColor text{1,1,1,.8}, description{1,1,1,.5}, disabled_text{1,1,1,.3};
    StyleColor idle{0,0,0,0}, hover{.1230252981,.1252803057,.1275353134,1};
    StyleColor pressed{.1315810233,.2001844049,.2785882354,1}, selected{.0812049285,.5099760294,1,.30};
    StyleColor focus{.0812049285,.5099760294,1,1};
    friend bool operator==(const ListItemPalette&, const ListItemPalette&) = default;
};
struct ListItemConfiguration final {
    ListItemVariant variant = ListItemVariant::mini;
    std::string label = "Label", description, accessibility_name, icon_name;
    double width = 0, height = 0, left_padding = 4, right_padding = 4;
    double icon_size = 14, spacing = 6, corner_radius = 4, chevron_size = 14;
    bool has_icon = false, enabled = true, interactive = true, selectable = true, selected = false;
    bool show_description = true, show_chevron = false, visible = true;
    ListItemPalette palette;
    friend bool operator==(const ListItemConfiguration&, const ListItemConfiguration&) = default;
};
struct ListItemSize final { double width = 0, height = 0; };
struct ListItemRect final { double x = 0, y = 0, width = 0, height = 0; };
struct ListItemGeometry final { ListItemRect surface, icon, label, description, chevron; };
struct ListItemInteraction final { bool hovered = false, pressed = false, focused = false; };
struct ListItemVisual final { StyleColor background, text, description, focus; bool focused = false; };

// The row's data, layout and visual state do not depend on a host widget or a list model.
class ListItem final {
public:
    [[nodiscard]] static bool validate(const ListItemConfiguration& value) noexcept {
        const auto number = [](double n, double max) { return std::isfinite(n) && n >= 0 && n <= max; };
        const auto color = [&](StyleColor c) { return number(c.red, 1) && number(c.green, 1) && number(c.blue, 1) && number(c.alpha, 1); };
        const auto& p = value.palette;
        return (value.variant == ListItemVariant::mini || value.variant == ListItemVariant::detail || value.variant == ListItemVariant::navigation ||
             value.variant == ListItemVariant::toggle || value.variant == ListItemVariant::checkable ||
             value.variant == ListItemVariant::action || value.variant == ListItemVariant::action_group || value.variant == ListItemVariant::stepper || value.variant == ListItemVariant::select || value.variant == ListItemVariant::inline_edit) &&
            valid_accessibility_text(value.label) && valid_accessibility_text(value.description) &&
            valid_accessibility_text(value.accessibility_name) && value.icon_name.size() <= 256 && valid_accessibility_text(value.icon_name) &&
            number(value.width, 8192) && number(value.height, 8192) && number(value.left_padding, 8192) && number(value.right_padding, 8192) &&
            number(value.icon_size, 256) && number(value.spacing, 8192) && number(value.corner_radius, 8192) && number(value.chevron_size, 256) &&
            color(p.text) && color(p.description) && color(p.disabled_text) && color(p.idle) && color(p.hover) &&
            color(p.pressed) && color(p.selected) && color(p.focus);
    }
    bool set_configuration(ListItemConfiguration value) {
        if (!validate(value)) return false;
        configuration_ = std::move(value); return true;
    }
    [[nodiscard]] const ListItemConfiguration& configuration() const noexcept { return configuration_; }
    [[nodiscard]] ListItemSize intrinsic_size() const noexcept {
        const auto& c = configuration_;
        const double width = c.variant == ListItemVariant::mini ? 170 : c.variant == ListItemVariant::detail ? 194 :
            c.variant == ListItemVariant::action || c.variant == ListItemVariant::action_group || c.variant == ListItemVariant::stepper || c.variant == ListItemVariant::select || c.variant == ListItemVariant::inline_edit ? 400 : 280;
        const double height = c.variant == ListItemVariant::mini ? 20 : 44;
        return {c.width > 0 ? c.width : width, c.height > 0 ? c.height : height};
    }
    [[nodiscard]] std::optional<ListItemGeometry> geometry(double width, double height) const noexcept {
        if (!std::isfinite(width) || !std::isfinite(height) || width < 0 || height < 0 || width > 8192 || height > 8192)
            return std::nullopt;
        const auto& c = configuration_; ListItemGeometry result; result.surface = {0,0,width,height};
        auto left = std::min(c.left_padding, width / 2);
        auto right = width - std::min(c.right_padding, width - left);
        if (c.show_chevron && c.chevron_size > 0) {
            const auto size = std::min({c.chevron_size, height, std::max(0., right - left)});
            result.chevron = {right - size, (height - size) / 2, size, size};
            right = std::max(left, right - size - std::min(c.spacing, std::max(0., right - size - left)));
        }
        if (c.has_icon && c.icon_size > 0) {
            const auto size = std::min({c.icon_size, height, std::max(0., right - left)});
            result.icon = {left, (height - size) / 2, size, size};
            left += size + (size > 0 ? std::min(c.spacing, std::max(0., right - left - size)) : 0);
        }
        const auto available = std::max(0., right - left);
        const bool details = c.show_description && !c.description.empty() && height >= 32;
        if (details) {
            const auto y = (height - 30) / 2;
            result.label = {left, y, available, 16}; result.description = {left, y + 16, available, 14};
        } else {
            const auto text_height = std::min(16., height);
            result.label = {left, (height - text_height) / 2, available, text_height};
        }
        return result;
    }
    [[nodiscard]] ListItemVisual visual(ListItemInteraction interaction) const noexcept {
        const auto& c = configuration_; const auto& p = c.palette;
        const auto background = c.selected ? p.selected : interaction.pressed ? p.pressed : interaction.hovered ? p.hover : p.idle;
        return {background, c.enabled ? p.text : p.disabled_text, c.enabled ? p.description : p.disabled_text,
                p.focus, c.enabled && c.interactive && c.selectable && interaction.focused};
    }
private:
    ListItemConfiguration configuration_;
};

} // namespace lvui
