#pragma once

#include <LVUI/Table.h>

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>

namespace lvui {

enum class TableMergeAction { merge, split };
struct TableMergeEdit final {
    std::uint64_t revision = 0;
    TableMergeAction action = TableMergeAction::merge;
    // Split uses the row and column of any cell and requires a 1 x 1 range.
    TableMerge range;
};
class TableMergeEditor {
public:
    virtual ~TableMergeEditor() = default;
    [[nodiscard]] virtual TableResult<std::uint64_t> apply(const TableMergeEdit&) = 0;
};

// Plans a revision-checked, atomic candidate without modifying the source.
// Cell values stay in their original cells when their presentation is merged.
[[nodiscard]] inline TableResult<TableSnapshot> prepare_table_merge(
        std::shared_ptr<const TableSnapshot> source, const TableMergeEdit& edit) {
    auto index = TableIndex::create(source); if (!index) return std::unexpected(index.error());
    if (source->revision != edit.revision)
        return std::unexpected(TableError{TableErrorCode::stale, "Table changed before the merge edit"});
    const auto invalid = [](std::string message) -> TableResult<TableSnapshot> {
        return std::unexpected(TableError{TableErrorCode::invalid, std::move(message)});
    };
    const auto& range = edit.range;
    if (unsigned(edit.action) > unsigned(TableMergeAction::split) || !range.row_count || !range.column_count ||
        range.row >= source->rows.size() || range.column >= source->columns.size() ||
        range.row_count > source->rows.size() - range.row || range.column_count > source->columns.size() - range.column)
        return invalid("Invalid table merge edit");
    auto result = *source;
    if (edit.action == TableMergeAction::split) {
        if (range.row_count != 1 || range.column_count != 1) return invalid("A split requires one cell position");
        const auto existing = index->merge_at(range.row, range.column);
        if (!existing) return result;
        std::erase(result.merges, *existing);
    } else {
        if (range.row_count == 1 && range.column_count == 1) return invalid("A merge requires more than one cell");
        if (index->merge_at(range.row, range.column) == range) return result;
        const auto intersects = [&](const TableMerge& other) {
            return range.row < other.row + other.row_count && other.row < range.row + range.row_count &&
                   range.column < other.column + other.column_count && other.column < range.column + range.column_count;
        };
        std::erase_if(result.merges, intersects); result.merges.push_back(range);
    }
    std::ranges::sort(result.merges, [](const TableMerge& a, const TableMerge& b) {
        return std::tie(a.row, a.column, a.row_count, a.column_count) <
               std::tie(b.row, b.column, b.row_count, b.column_count);
    });
    if (auto valid = TableIndex::create(std::make_shared<const TableSnapshot>(result)); !valid)
        return std::unexpected(valid.error());
    return result;
}

// Optional adapter; applications can supply their own persistent merge editor.
class TableMemoryMergeEditor final : public TableMergeEditor {
public:
    explicit TableMemoryMergeEditor(std::shared_ptr<TableMemory> source) : source_(std::move(source)) {
        if (!source_) throw std::invalid_argument("A table merge source is required");
    }
    [[nodiscard]] TableResult<std::uint64_t> apply(const TableMergeEdit& edit) override {
        auto source = source_->snapshot(); if (!source) return std::unexpected(source.error());
        auto candidate = prepare_table_merge(*source, edit); if (!candidate) return std::unexpected(candidate.error());
        return source_->replace_snapshot(std::move(*candidate), edit.revision);
    }
private:
    std::shared_ptr<TableMemory> source_;
};

} // namespace lvui
