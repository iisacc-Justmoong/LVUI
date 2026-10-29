#pragma once

#include "Table.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lvui {

// Clipboard serialization is separate from the platform clipboard and from the
// application's editor. A prepared paste can therefore be reviewed and submitted
// as one revision-checked edit, including by non-graphical clients.
inline constexpr std::size_t table_clipboard_maximum_bytes = 1024 * 1024;
inline constexpr std::size_t table_clipboard_maximum_cells = 4096;
struct TableClipboardData final {
    std::vector<std::vector<std::string>> rows;
    friend bool operator==(const TableClipboardData&, const TableClipboardData&) = default;
};

[[nodiscard]] inline TableResult<TableClipboardData> parse_table_clipboard(std::string_view text) {
    const auto invalid = [](std::string message) -> TableResult<TableClipboardData> {
        return std::unexpected(TableError{TableErrorCode::invalid, std::move(message)});
    };
    if (text.size() > table_clipboard_maximum_bytes) return invalid("Table clipboard exceeds its byte limit");
    TableClipboardData result;
    std::vector<std::string> row;
    std::string field;
    std::size_t cells = 0, width = 0, at = 0;
    bool quoted = false, closed = false, at_start = true;
    const auto finish_field = [&]() -> bool {
        if (!valid_accessibility_text(field) || ++cells > table_clipboard_maximum_cells || row.size() >= table_maximum_columns)
            return false;
        row.push_back(std::move(field)); field.clear(); quoted = closed = false; at_start = true; return true;
    };
    const auto finish_row = [&]() -> bool {
        if (!finish_field() || result.rows.size() >= table_maximum_rows) return false;
        if (result.rows.empty()) width = row.size();
        else if (row.size() != width) return false;
        result.rows.push_back(std::move(row)); row.clear(); return true;
    };
    while (at < text.size()) {
        const char ch = text[at++];
        if (quoted) {
            if (ch == '"') {
                if (at < text.size() && text[at] == '"') { field.push_back('"'); ++at; }
                else { quoted = false; closed = true; }
            } else field.push_back(ch);
            continue;
        }
        if (ch == '\t') { if (!finish_field()) return invalid("Invalid table clipboard field"); continue; }
        if (ch == '\r' || ch == '\n') {
            if (ch == '\r' && at < text.size() && text[at] == '\n') ++at;
            if (!finish_row()) return invalid("Invalid or uneven table clipboard row");
            continue;
        }
        if (closed) return invalid("Unexpected text after a quoted table clipboard field");
        if (ch == '"') {
            if (!at_start) return invalid("Unexpected quote in a table clipboard field");
            quoted = true; at_start = false;
        } else { field.push_back(ch); at_start = false; }
    }
    if (quoted) return invalid("Unclosed quoted table clipboard field");
    // A final record terminator completes the last row; it does not add one.
    const bool final_record = text.empty() || (text.back() != '\r' && text.back() != '\n') || quoted || closed || !at_start || !row.empty();
    if (final_record && !finish_row()) return invalid("Invalid or uneven table clipboard row");
    if (result.rows.empty()) return invalid("Table clipboard contains no cells");
    return result;
}

[[nodiscard]] inline TableResult<std::string> serialize_table_clipboard(
        std::shared_ptr<const TableSnapshot> snapshot, TableRange range) {
    auto index = TableIndex::create(snapshot); if (!index) return std::unexpected(index.error());
    const auto invalid = [](std::string message) -> TableResult<std::string> {
        return std::unexpected(TableError{TableErrorCode::invalid, std::move(message)});
    };
    if (range.first_row > range.last_row || range.last_row >= snapshot->rows.size() ||
        range.first_column > range.last_column || range.last_column >= snapshot->columns.size())
        return invalid("Table clipboard range is outside the snapshot");
    const auto rows = range.last_row - range.first_row + 1, columns = range.last_column - range.first_column + 1;
    if (rows > table_clipboard_maximum_cells / columns) return invalid("Table clipboard exceeds its cell limit");
    std::string output;
    const auto append = [&](std::string_view part) -> bool {
        if (part.size() > table_clipboard_maximum_bytes - output.size()) return false;
        output.append(part); return true;
    };
    for (std::size_t r = range.first_row; r <= range.last_row; ++r) {
        if (r != range.first_row && !append("\n")) return invalid("Table clipboard exceeds its byte limit");
        for (std::size_t c = range.first_column; c <= range.last_column; ++c) {
            if (c != range.first_column && !append("\t")) return invalid("Table clipboard exceeds its byte limit");
            const auto& cell = snapshot->rows[r].cells[c].text;
            // An empty last row in a one-column selection must not look like
            // a terminal record separator when parsed by another application.
            const bool quote = cell.find_first_of("\t\r\n\"") != std::string::npos ||
                (cell.empty() && r == range.last_row && r != range.first_row && columns == 1);
            if (quote && !append("\"")) return invalid("Table clipboard exceeds its byte limit");
            for (char ch : cell) {
                if (ch == '"' && !append("\"")) return invalid("Table clipboard exceeds its byte limit");
                if (!append(std::string_view(&ch, 1))) return invalid("Table clipboard exceeds its byte limit");
            }
            if (quote && !append("\"")) return invalid("Table clipboard exceeds its byte limit");
        }
    }
    return output;
}

[[nodiscard]] inline TableResult<TableEdit> prepare_table_paste(
        std::shared_ptr<const TableSnapshot> snapshot, TableRange selection, const TableClipboardData& input) {
    auto index = TableIndex::create(snapshot); if (!index) return std::unexpected(index.error());
    const auto invalid = [](std::string message) -> TableResult<TableEdit> {
        return std::unexpected(TableError{TableErrorCode::invalid, std::move(message)});
    };
    if (selection.first_row > selection.last_row || selection.last_row >= snapshot->rows.size() ||
        selection.first_column > selection.last_column || selection.last_column >= snapshot->columns.size() ||
        input.rows.empty() || input.rows.front().empty()) return invalid("Invalid table paste selection or data");
    const auto input_rows = input.rows.size(), input_columns = input.rows.front().size();
    if (input_rows > table_clipboard_maximum_cells / input_columns) return invalid("Table paste exceeds its cell limit");
    std::size_t bytes = 0;
    for (const auto& row : input.rows) {
        if (row.size() != input_columns) return invalid("Table paste rows have different lengths");
        for (const auto& field : row) {
            if (!valid_accessibility_text(field) || field.size() > table_clipboard_maximum_bytes - bytes)
                return invalid("Invalid or oversized table paste field");
            bytes += field.size();
        }
    }
    const auto selection_rows = selection.last_row - selection.first_row + 1;
    const auto selection_columns = selection.last_column - selection.first_column + 1;
    const bool fill = input_rows == 1 && input_columns == 1;
    const bool expand = selection_rows == 1 && selection_columns == 1;
    if (!fill && !expand && (selection_rows != input_rows || selection_columns != input_columns))
        return invalid("Table paste dimensions do not match the selected range");
    const auto rows = expand ? input_rows : selection_rows, columns = expand ? input_columns : selection_columns;
    if (rows > table_clipboard_maximum_cells / columns || rows > snapshot->rows.size() - selection.first_row ||
        columns > snapshot->columns.size() - selection.first_column) return invalid("Table paste exceeds the target bounds");
    if (fill && input.rows[0][0].size() > table_clipboard_maximum_bytes / (rows * columns))
        return invalid("Expanded table paste exceeds its byte limit");
    TableEdit edit{.revision=snapshot->revision}; edit.cells.reserve(rows * columns);
    for (std::size_t r = 0; r < rows; ++r) for (std::size_t c = 0; c < columns; ++c) {
        const auto& row = snapshot->rows[selection.first_row + r];
        const auto& column = snapshot->columns[selection.first_column + c];
        const auto& cell = row.cells[selection.first_column + c];
        if (!row.enabled || !column.enabled || !column.editable || !cell.enabled || !cell.editable)
            return invalid("Table paste contains a disabled or read-only cell");
        edit.cells.push_back({row.key, column.key, input.rows[fill ? 0 : r][fill ? 0 : c]});
    }
    return edit;
}

} // namespace lvui
