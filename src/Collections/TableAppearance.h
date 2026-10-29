#pragma once

#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>

#include <cmath>
#include <string>

namespace lvui {

struct TablePalette final {
    StyleColor background{.1176470588, .1176470588, .1176470588, 1};
    StyleColor divider{.1555851549, .1570725590, .1585599482, 1};
    StyleColor header_text{1, 1, 1, .6}, cell_text{1, 1, 1, .8};
    StyleColor selection{.0812049285, .5099760294, 1, 31. / 255.};
    StyleColor current{.0812049285, .5099760294, 1, 1};
    friend bool operator==(const TablePalette&, const TablePalette&) = default;
};
struct TableAppearance final {
    double width = 660, height = 300, header_height = 24, cell_padding = 8;
    double fallback_column_width = 120, minimum_column_width = 32, font_size = 13;
    bool enabled = true, selection_enabled = true, keyboard_navigation = true;
    bool fit_flexible_columns = true, show_indicator = true;
    std::string accessibility_name = "Table";
    TablePalette palette;
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
        color(p.cell_text) && color(p.selection) && color(p.current);
}

} // namespace lvui
