#pragma once

#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>

#include <cmath>
#include <optional>
#include <string>

namespace lvui {

struct TableTypography final {
    double font_size=13, line_height=13;
    unsigned font_weight=500;
    friend bool operator==(const TableTypography&,const TableTypography&) = default;
};
[[nodiscard]] inline bool valid_table_typography(const std::optional<TableTypography>& value) noexcept {
    if(!value)return true;
    return std::isfinite(value->font_size)&&value->font_size>=1&&value->font_size<=256&&
        std::isfinite(value->line_height)&&value->line_height>=0&&value->line_height<=8192&&
        (value->font_weight==400||value->font_weight==500||value->font_weight==600||value->font_weight==700);
}
struct TablePalette final {
    StyleColor background{.1176470588, .1176470588, .1176470588, 1};
    StyleColor divider{figma_defaults::panel_surface_input};
    StyleColor header_text{1, 1, 1, .6}, cell_text{1, 1, 1, .8};
    StyleColor selection{figma_defaults::with_alpha(figma_defaults::accent_blue_default, 31. / 255.)};
    StyleColor current{figma_defaults::accent_blue_default};
    friend bool operator==(const TablePalette&, const TablePalette&) = default;
};
struct TableAppearance final {
    double width = 660, height = 300, header_height = 24, cell_padding = 8;
    double fallback_column_width = 120, minimum_column_width = 32, font_size = 13;
    bool enabled = true, selection_enabled = true, keyboard_navigation = true;
    bool fit_flexible_columns = true, show_indicator = true;
    std::string accessibility_name = "Table";
    TablePalette palette;
    // Unset preserves the existing C++ table typography.
    std::optional<TableTypography> header_typography, cell_typography;
    friend bool operator==(const TableAppearance&, const TableAppearance&) = default;
};
[[nodiscard]] inline bool valid_table_appearance(const TableAppearance& value) noexcept {
    const auto bound = [](double n, double low, double high) { return std::isfinite(n) && n >= low && n <= high; };
    const auto color = [&](StyleColor c) { return bound(c.red, 0, 1) && bound(c.green, 0, 1) && bound(c.blue, 0, 1) && bound(c.alpha, 0, 1); };
    const auto& p = value.palette;
    return bound(value.width, 32, 8192) && bound(value.height, 19, 8192) && bound(value.header_height, 1, 8191) &&
        value.header_height < value.height && bound(value.cell_padding, 0, 8192) && bound(value.font_size, 1, 256) &&
        bound(value.minimum_column_width, 32, 8192) && bound(value.fallback_column_width, value.minimum_column_width, 8192) &&
        valid_accessibility_text(value.accessibility_name) && color(p.background) && color(p.divider) && color(p.header_text) &&
        color(p.cell_text) && color(p.selection) && color(p.current) &&
        valid_table_typography(value.header_typography) && valid_table_typography(value.cell_typography);
}

} // namespace lvui
