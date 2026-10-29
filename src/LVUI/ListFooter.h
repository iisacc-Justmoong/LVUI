#pragma once

#include "ToolbarButton.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>

namespace lvui {

class ListFooterCommand {
public:
    virtual ~ListFooterCommand() = default;
    virtual void invoke() const = 0;
};
struct ListFooterSlot final {
    std::string icon_name, text, accessibility_name;
    bool enabled = true, visible = true, menu_indicator = false;
    std::shared_ptr<const ListFooterCommand> command;
    friend bool operator==(const ListFooterSlot&, const ListFooterSlot&) = default;
};
struct ListFooterConfiguration final {
    std::array<ListFooterSlot, 3> slots{{
        {.icon_name="addFile", .accessibility_name="Add file"},
        {.icon_name="generaldelete", .accessibility_name="Delete"},
        {.icon_name="settings", .accessibility_name="Settings", .menu_indicator=true}}};
    double width = 0, height = 0, horizontal_padding = 2, vertical_padding = 2, spacing = 0;
    double icon_size = 18, button_height = 22, font_size = 12, corner_radius = 8;
    bool enabled = true, interactive = true;
    ToolbarButtonPalette palette;
    friend bool operator==(const ListFooterConfiguration&, const ListFooterConfiguration&) = default;
};
struct ListFooterRect final { double x = 0, y = 0, width = 0, height = 0; };
struct ListFooterGeometry final {
    double width = 0, height = 0;
    std::array<ListFooterRect, 3> slots;
};

class ListFooter final {
public:
    [[nodiscard]] static bool validate(const ListFooterConfiguration& value) noexcept {
        const auto bound = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
        for (const auto& slot : value.slots)
            if (slot.icon_name.size() > 256 || !valid_accessibility_text(slot.icon_name) || !valid_accessibility_text(slot.text) ||
                !valid_accessibility_text(slot.accessibility_name)) return false;
        ToolbarButtonConfiguration button; button.palette = value.palette; button.icon_size = value.icon_size;
        button.height = value.button_height; button.font_size = value.font_size; button.corner_radius = value.corner_radius;
        return ToolbarButton::validate(button) && bound(value.width, 0, 8192) && bound(value.height, 0, 8192) &&
            bound(value.horizontal_padding, 0, 8192) && bound(value.vertical_padding, 0, 8192) && bound(value.spacing, 0, 8192) &&
            2 * value.vertical_padding + value.button_height <= 8192;
    }
    [[nodiscard]] static std::optional<ListFooterGeometry> geometry(
            const ListFooterConfiguration& value, const std::array<double, 3>& measured_text = {}) noexcept {
        if (!validate(value)) return std::nullopt;
        std::array<double, 3> widths{}; double total = 0; std::size_t visible = 0;
        for (std::size_t i = 0; i < value.slots.size(); ++i) {
            if (!std::isfinite(measured_text[i]) || measured_text[i] < 0 || measured_text[i] > 8192) return std::nullopt;
            const auto& slot = value.slots[i]; if (!slot.visible) continue; ++visible;
            const auto icon = slot.icon_name.empty() ? 0. : value.icon_size;
            const auto text = slot.text.empty() ? 0. : measured_text[i];
            const auto gap = icon > 0 && text > 0 ? 4. : 0.;
            const auto menu = slot.menu_indicator ? 18. : 0.;
            widths[i] = std::max(value.button_height, 4. + icon + gap + text + menu); total += widths[i];
        }
        const auto gaps = visible > 1 ? (visible - 1) * value.spacing : 0.;
        const auto natural_width = 2 * value.horizontal_padding + total + gaps;
        if (natural_width > 8192) return std::nullopt;
        ListFooterGeometry result; result.width = value.width ? value.width : natural_width;
        result.height = value.height ? value.height : value.button_height + 2 * value.vertical_padding;
        const auto inset = std::min(value.horizontal_padding, result.width / 2);
        const auto available = std::max(0., result.width - 2 * inset);
        const auto gap = visible > 1 ? std::min(value.spacing, available / double(visible - 1)) : 0.;
        const auto remaining = std::max(0., available - gap * (visible ? visible - 1 : 0));
        const auto scale = total > 0 ? std::min(1., remaining / total) : 0.;
        const auto top = std::min(value.vertical_padding, result.height / 2);
        const auto height = std::min(value.button_height, std::max(0., result.height - 2 * top));
        double x = inset; bool first = true;
        for (std::size_t i = 0; i < value.slots.size(); ++i) if (value.slots[i].visible) {
            if (!first) x += gap; first = false;
            result.slots[i] = {x, (result.height - height) / 2, widths[i] * scale, height}; x += widths[i] * scale;
        }
        return result;
    }
};

} // namespace lvui
