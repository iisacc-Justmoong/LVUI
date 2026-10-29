#pragma once

#include "Table.h"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace lvui {

struct TableInsertRows final { std::size_t index = 0; std::vector<TableRow> rows; };
struct TableRemoveRows final { std::vector<std::string> keys; };
struct TableInsertColumns final {
    std::size_t index = 0;
    std::vector<TableColumn> columns;
    // One vector per existing row, with one cell for each inserted column.
    std::vector<std::vector<TableCell>> cells;
};
struct TableRemoveColumns final { std::vector<std::string> keys; };
struct TableRowSize final { std::string key; double height = 24; };
struct TableColumnSize final { std::string key; double width = 0; };
struct TableResizeRows final { std::vector<TableRowSize> rows; };
struct TableResizeColumns final { std::vector<TableColumnSize> columns; };
using TableStructureOperation = std::variant<TableInsertRows, TableRemoveRows, TableInsertColumns,
                                             TableRemoveColumns, TableResizeRows, TableResizeColumns>;
struct TableStructureEdit final { std::uint64_t revision = 0; std::vector<TableStructureOperation> operations; };
class TableStructureEditor {
public:
    virtual ~TableStructureEditor() = default;
    [[nodiscard]] virtual TableResult<std::uint64_t> apply(const TableStructureEdit&) = 0;
};

// An application may preview and validate an entire structure edit before
// asking its own editor to commit the same revision-checked request.
[[nodiscard]] inline TableResult<TableSnapshot> prepare_table_structure(
        std::shared_ptr<const TableSnapshot> snapshot, const TableStructureEdit& edit) {
    const auto invalid = [](std::string message) -> TableResult<TableSnapshot> {
        return std::unexpected(TableError{TableErrorCode::invalid, std::move(message)});
    };
    auto initial = TableIndex::create(snapshot); if (!initial) return std::unexpected(initial.error());
    if (snapshot->revision != edit.revision) return std::unexpected(TableError{TableErrorCode::stale, "Table changed before the structure edit"});
    if (edit.operations.size() > 256) return invalid("Table structure edit exceeds its operation limit");
    auto candidate = std::make_shared<TableSnapshot>(*snapshot);
    for (const auto& operation : edit.operations) {
        auto index = TableIndex::create(candidate); if (!index) return std::unexpected(index.error());
        auto applied = std::visit([&](const auto& change) -> TableResult<void> {
            using T = std::decay_t<decltype(change)>;
            if constexpr (std::same_as<T, TableInsertRows>) {
                if (change.index > candidate->rows.size() || change.rows.size() > table_maximum_rows - candidate->rows.size() ||
                    (!candidate->columns.empty() && candidate->rows.size() + change.rows.size() > table_maximum_cells / candidate->columns.size()))
                    return std::unexpected(TableError{TableErrorCode::invalid, "Inserted table rows exceed bounds"});
                for (const auto& row : change.rows) if (row.cells.size() != candidate->columns.size())
                    return std::unexpected(TableError{TableErrorCode::invalid, "Inserted row has the wrong cell count"});
                candidate->rows.insert(candidate->rows.begin() + std::ptrdiff_t(change.index), change.rows.begin(), change.rows.end());
            } else if constexpr (std::same_as<T, TableRemoveRows>) {
                if (change.keys.size() > candidate->rows.size()) return std::unexpected(TableError{TableErrorCode::invalid, "Too many removed rows"});
                std::unordered_set<std::size_t> remove; remove.reserve(change.keys.size());
                for (const auto& key : change.keys) {
                    auto found = index->row(key);
                    if (!found || !remove.insert(*found).second) return std::unexpected(TableError{TableErrorCode::invalid, "Missing or duplicate removed row"});
                }
                std::vector<TableRow> rows; rows.reserve(candidate->rows.size() - remove.size());
                for (std::size_t i = 0; i < candidate->rows.size(); ++i) if (!remove.contains(i)) rows.push_back(std::move(candidate->rows[i]));
                candidate->rows = std::move(rows);
            } else if constexpr (std::same_as<T, TableInsertColumns>) {
                if (change.index > candidate->columns.size() || change.columns.size() > table_maximum_columns - candidate->columns.size() ||
                    change.cells.size() != candidate->rows.size() ||
                    (!candidate->rows.empty() && candidate->columns.size() + change.columns.size() > table_maximum_cells / candidate->rows.size()))
                    return std::unexpected(TableError{TableErrorCode::invalid, "Inserted table columns exceed bounds"});
                for (const auto& cells : change.cells) if (cells.size() != change.columns.size())
                    return std::unexpected(TableError{TableErrorCode::invalid, "Inserted column has the wrong cell count"});
                candidate->columns.insert(candidate->columns.begin() + std::ptrdiff_t(change.index), change.columns.begin(), change.columns.end());
                for (std::size_t i = 0; i < candidate->rows.size(); ++i)
                    candidate->rows[i].cells.insert(candidate->rows[i].cells.begin() + std::ptrdiff_t(change.index), change.cells[i].begin(), change.cells[i].end());
            } else if constexpr (std::same_as<T, TableRemoveColumns>) {
                if (change.keys.size() > candidate->columns.size()) return std::unexpected(TableError{TableErrorCode::invalid, "Too many removed columns"});
                std::unordered_set<std::size_t> remove; remove.reserve(change.keys.size());
                for (const auto& key : change.keys) {
                    auto found = index->column(key);
                    if (!found || !remove.insert(*found).second) return std::unexpected(TableError{TableErrorCode::invalid, "Missing or duplicate removed column"});
                }
                std::vector<TableColumn> columns; columns.reserve(candidate->columns.size() - remove.size());
                for (std::size_t i = 0; i < candidate->columns.size(); ++i) if (!remove.contains(i)) columns.push_back(std::move(candidate->columns[i]));
                for (auto& row : candidate->rows) {
                    std::vector<TableCell> cells; cells.reserve(row.cells.size() - remove.size());
                    for (std::size_t i = 0; i < row.cells.size(); ++i) if (!remove.contains(i)) cells.push_back(std::move(row.cells[i]));
                    row.cells = std::move(cells);
                }
                candidate->columns = std::move(columns);
            } else if constexpr (std::same_as<T, TableResizeRows>) {
                if (change.rows.size() > candidate->rows.size()) return std::unexpected(TableError{TableErrorCode::invalid, "Too many resized rows"});
                std::unordered_set<std::size_t> resized; resized.reserve(change.rows.size());
                for (const auto& row : change.rows) {
                    auto found = index->row(row.key);
                    if (!found || !resized.insert(*found).second) return std::unexpected(TableError{TableErrorCode::invalid, "Missing or duplicate resized row"});
                    candidate->rows[*found].height = row.height;
                }
            } else if constexpr (std::same_as<T, TableResizeColumns>) {
                if (change.columns.size() > candidate->columns.size()) return std::unexpected(TableError{TableErrorCode::invalid, "Too many resized columns"});
                std::unordered_set<std::size_t> resized; resized.reserve(change.columns.size());
                for (const auto& column : change.columns) {
                    auto found = index->column(column.key);
                    if (!found || !resized.insert(*found).second) return std::unexpected(TableError{TableErrorCode::invalid, "Missing or duplicate resized column"});
                    candidate->columns[*found].width = column.width;
                }
            }
            return {};
        }, operation);
        if (!applied) return std::unexpected(applied.error());
        if (auto valid = TableIndex::create(candidate); !valid) return std::unexpected(valid.error());
    }
    return std::move(*candidate);
}

class TableMemoryStructureEditor final : public TableStructureEditor {
public:
    explicit TableMemoryStructureEditor(std::shared_ptr<TableMemory> source) : source_(std::move(source)) {
        if (!source_) throw std::invalid_argument("A table structure source is required");
    }
    [[nodiscard]] TableResult<std::uint64_t> apply(const TableStructureEdit& edit) override {
        auto snapshot = source_->snapshot(); if (!snapshot) return std::unexpected(snapshot.error());
        auto candidate = prepare_table_structure(*snapshot, edit); if (!candidate) return std::unexpected(candidate.error());
        return source_->replace(std::move(candidate->columns), std::move(candidate->rows), edit.revision);
    }
private:
    std::shared_ptr<TableMemory> source_;
};

} // namespace lvui
