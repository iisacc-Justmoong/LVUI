#pragma once

#include <LVUI/Text.h>

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace lvui {

// Application-supplied text keeps table labels independent of a locale provider.
// Template fields use the exact placeholders documented in NativeOwnedTable.md.
struct TableText final {
    std::string unnamed_column = "Column {column}";
    std::string cell_name = "{column}, row {row}";
    std::string merged_description = "Spans {rows} rows and {columns} columns";
    std::string add_row_after = "Add row after row {row}";
    std::string add_column_after = "Add column after column {column}";
    std::string add_first_row = "Add first row", add_first_column = "Add first column";
    std::string edit_cell = "Edit table cell", structure_menu = "Table structure";
    std::string delete_row = "Delete row", delete_column = "Delete column";
    std::string header_group = "Table header", sortable_column = "Sortable";
    std::string sorted_ascending = "Sorted ascending", sorted_descending = "Sorted descending";
    friend bool operator==(const TableText&, const TableText&) = default;
};

namespace detail {
[[nodiscard]] inline bool valid_table_pattern(std::string_view pattern,
                                               std::initializer_list<std::string_view> fields) noexcept {
    if (pattern.empty() || pattern.size() > 512 || !valid_accessibility_text(pattern)) return false;
    unsigned seen = 0;
    for (std::size_t i = 0; i < pattern.size();) {
        if (pattern[i] == '{') {
            if (i + 1 < pattern.size() && pattern[i + 1] == '{') { i += 2; continue; }
            const auto end = pattern.find('}', i + 1); if (end == pattern.npos) return false;
            const auto field = pattern.substr(i + 1, end - i - 1);
            unsigned bit = 1; bool found = false;
            for (const auto expected : fields) { if (field == expected) { found = true; break; } bit <<= 1; }
            if (!found || (seen & bit)) return false;
            seen |= bit; i = end + 1;
        } else if (pattern[i] == '}') {
            if (i + 1 >= pattern.size() || pattern[i + 1] != '}') return false;
            i += 2;
        } else ++i;
    }
    return seen == (1u << fields.size()) - 1;
}

[[nodiscard]] inline std::string format_table_text(
        std::string_view pattern, std::initializer_list<std::pair<std::string_view,std::string_view>> fields) {
    std::string result; result.reserve(pattern.size() + 32);
    for (std::size_t i = 0; i < pattern.size();) {
        if (pattern[i] == '{') {
            if (i + 1 < pattern.size() && pattern[i + 1] == '{') { result += '{'; i += 2; continue; }
            const auto end = pattern.find('}', i + 1); const auto field = pattern.substr(i + 1, end - i - 1);
            for (const auto& [name, value] : fields) if (name == field) { result.append(value); break; }
            i = end + 1;
        } else if (pattern[i] == '}' && i + 1 < pattern.size() && pattern[i + 1] == '}') {
            result += '}'; i += 2;
        } else result += pattern[i++];
    }
    return result;
}
} // namespace detail

[[nodiscard]] inline bool valid_table_text(const TableText& text) noexcept {
    const auto literal = [](std::string_view value) { return !value.empty() && value.size() <= 512 && valid_accessibility_text(value); };
    return detail::valid_table_pattern(text.unnamed_column, {"column"}) &&
        detail::valid_table_pattern(text.cell_name, {"column", "row"}) &&
        detail::valid_table_pattern(text.merged_description, {"rows", "columns"}) &&
        detail::valid_table_pattern(text.add_row_after, {"row"}) &&
        detail::valid_table_pattern(text.add_column_after, {"column"}) &&
        literal(text.add_first_row) && literal(text.add_first_column) && literal(text.edit_cell) &&
        literal(text.structure_menu) && literal(text.delete_row) && literal(text.delete_column) &&
        literal(text.header_group) && literal(text.sortable_column) && literal(text.sorted_ascending) && literal(text.sorted_descending);
}

} // namespace lvui
