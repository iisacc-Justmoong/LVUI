#pragma once

#include <LVUI/Table.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lvui {

enum class TableSortOrder { ascending, descending };
struct TableSortPlan final {
    std::uint64_t revision = 0;
    std::string column;
    TableSortOrder order = TableSortOrder::ascending;
    std::vector<std::string> rows;
};

// Sorting policy and document mutation are independent. A policy inspects a
// retained immutable snapshot and returns keys; an editor atomically accepts
// or rejects the resulting revision-checked permutation.
class TableSorter {
public:
    virtual ~TableSorter() = default;
    [[nodiscard]] virtual TableResult<std::vector<std::string>> order(
        const TableSnapshot&, std::size_t column, TableSortOrder) const = 0;
};
class TableOrderEditor {
public:
    virtual ~TableOrderEditor() = default;
    [[nodiscard]] virtual TableResult<std::uint64_t> reorder(const TableSortPlan&) = 0;
};

class TableTextSorter final : public TableSorter {
public:
    [[nodiscard]] TableResult<std::vector<std::string>> order(
            const TableSnapshot& snapshot, std::size_t column, TableSortOrder direction) const override {
        if (column >= snapshot.columns.size() || unsigned(direction) > unsigned(TableSortOrder::descending))
            return std::unexpected(TableError{TableErrorCode::invalid, "Invalid table sort request"});
        if (snapshot.rows.size() > table_maximum_rows || snapshot.columns.size() > table_maximum_columns ||
            (!snapshot.columns.empty() && snapshot.rows.size() > table_maximum_cells / snapshot.columns.size()))
            return std::unexpected(TableError{TableErrorCode::invalid, "Invalid table sort dimensions"});
        for (const auto& row : snapshot.rows) if (row.cells.size() != snapshot.columns.size())
            return std::unexpected(TableError{TableErrorCode::invalid, "Invalid table sort row"});
        std::vector<std::size_t> indices(snapshot.rows.size()); std::iota(indices.begin(), indices.end(), 0);
        std::stable_sort(indices.begin(), indices.end(), [&](std::size_t left, std::size_t right) {
            const auto& a = snapshot.rows[left].cells[column].text; const auto& b = snapshot.rows[right].cells[column].text;
            return direction == TableSortOrder::ascending ? a < b : b < a;
        });
        std::vector<std::string> rows; rows.reserve(indices.size());
        for (auto row : indices) rows.push_back(snapshot.rows[row].key);
        return rows;
    }
};

[[nodiscard]] inline TableResult<void> validate_table_order(const TableIndex& index, const TableSortPlan& plan) {
    const auto& snapshot = index.snapshot(); const auto column = index.column(plan.column);
    if (plan.revision != snapshot.revision) return std::unexpected(TableError{TableErrorCode::stale, "Table changed before sorting"});
    if (!column || !snapshot.columns[*column].enabled || unsigned(plan.order) > unsigned(TableSortOrder::descending) ||
        plan.rows.size() != snapshot.rows.size()) return std::unexpected(TableError{TableErrorCode::invalid, "Invalid table sort permutation"});
    std::vector<bool> seen(snapshot.rows.size(), false);
    for (const auto& key : plan.rows) {
        const auto row = index.row(key);
        if (!row || seen[*row]) return std::unexpected(TableError{TableErrorCode::invalid, "Duplicate or missing row in table sort"});
        seen[*row] = true;
    }
    return {};
}

[[nodiscard]] inline TableResult<TableSortPlan> prepare_table_sort(
        std::shared_ptr<const TableSnapshot> snapshot, std::string column, TableSortOrder direction,
        const TableSorter& sorter) {
    auto index = TableIndex::create(snapshot); if (!index) return std::unexpected(index.error());
    auto position = index->column(column);
    if (!position || !snapshot->columns[*position].enabled || unsigned(direction) > unsigned(TableSortOrder::descending))
        return std::unexpected(TableError{TableErrorCode::invalid, "Invalid table sort column or direction"});
    auto rows = sorter.order(*snapshot, *position, direction); if (!rows) return std::unexpected(rows.error());
    TableSortPlan plan{snapshot->revision, std::move(column), direction, std::move(*rows)};
    if (auto valid = validate_table_order(*index, plan); !valid) return std::unexpected(valid.error());
    return plan;
}

// The optional memory adapter keeps TableMemory's replace operation as the
// sole atomic commit point. Other stores implement TableOrderEditor directly.
class TableMemoryOrderEditor final : public TableOrderEditor {
public:
    explicit TableMemoryOrderEditor(std::shared_ptr<TableMemory> source) : source_(std::move(source)) {
        if (!source_) throw std::invalid_argument("A table ordering source is required");
    }
    [[nodiscard]] TableResult<std::uint64_t> reorder(const TableSortPlan& plan) override {
        auto snapshot = source_->snapshot(); if (!snapshot) return std::unexpected(snapshot.error());
        auto index = TableIndex::create(*snapshot); if (!index) return std::unexpected(index.error());
        if (auto valid = validate_table_order(*index, plan); !valid) return std::unexpected(valid.error());
        std::vector<TableRow> rows; rows.reserve(plan.rows.size());
        for (const auto& key : plan.rows) rows.push_back((*snapshot)->rows[*index->row(key)]);
        return source_->replace((*snapshot)->columns, std::move(rows), plan.revision);
    }
private:
    std::shared_ptr<TableMemory> source_;
};

} // namespace lvui
