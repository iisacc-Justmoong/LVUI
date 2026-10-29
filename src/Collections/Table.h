#pragma once

#include <LVUI/Text.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lvui {

inline constexpr std::size_t table_maximum_rows = 65536, table_maximum_columns = 1024, table_maximum_cells = 1000000;
enum class TableErrorCode { invalid, not_found, stale, exhausted, wrong_thread, busy };
struct TableError final { TableErrorCode code; std::string message; };
template<class T> using TableResult = std::expected<T, TableError>;

struct TableColumn final {
    std::string key, title;
    double width = 0; // Zero requests a flexible column; an explicit width is at least 32.
    bool enabled = true, editable = true;
    friend bool operator==(const TableColumn&, const TableColumn&) = default;
};
struct TableCell final {
    std::string text, value_type = "text";
    bool enabled = true, editable = true;
    friend bool operator==(const TableCell&, const TableCell&) = default;
};
struct TableRow final {
    std::string key;
    std::vector<TableCell> cells;
    double height = 24;
    bool enabled = true;
    friend bool operator==(const TableRow&, const TableRow&) = default;
};
struct TableSnapshot final {
    std::vector<TableColumn> columns;
    std::vector<TableRow> rows;
    std::uint64_t revision = 0;
};
class TableSource {
public:
    virtual ~TableSource() = default;
    [[nodiscard]] virtual TableResult<std::shared_ptr<const TableSnapshot>> snapshot() const = 0;
};
struct TableCellChange final { std::string row, column, text; };
struct TableEdit final { std::uint64_t revision = 0; std::vector<TableCellChange> cells; };
class TableEditor {
public:
    virtual ~TableEditor() = default;
    [[nodiscard]] virtual TableResult<std::uint64_t> edit(const TableEdit&) = 0;
};

// Validated immutable data and key lookup. Custom sources can retain their own
// model; a snapshot is an application-facing boundary, not a GUI widget.
class TableIndex final {
public:
    [[nodiscard]] static TableResult<TableIndex> create(std::shared_ptr<const TableSnapshot> snapshot) {
        if (!snapshot || snapshot->rows.size() > table_maximum_rows || snapshot->columns.size() > table_maximum_columns ||
            (!snapshot->columns.empty() && snapshot->rows.size() > table_maximum_cells / snapshot->columns.size()))
            return error(TableErrorCode::invalid, "Invalid table dimensions");
        TableIndex result; result.snapshot_ = std::move(snapshot);
        result.columns_.reserve(result.snapshot_->columns.size()); result.rows_.reserve(result.snapshot_->rows.size());
        std::size_t bytes = 0;
        const auto append = [&](std::string_view text, std::size_t limit) {
            if (text.size() > limit || !valid_accessibility_text(text) || text.size() > 64 * 1024 * 1024 - bytes) return false;
            bytes += text.size(); return true;
        };
        for (std::size_t i = 0; i < result.snapshot_->columns.size(); ++i) {
            const auto& column = result.snapshot_->columns[i];
            if (column.key.empty() || !append(column.key, 1024) || !append(column.title, 65536) ||
                !std::isfinite(column.width) || column.width < 0 || column.width > 8192 ||
                (column.width > 0 && column.width < 32) || !result.columns_.emplace(column.key, i).second)
                return error(TableErrorCode::invalid, "Invalid table column");
        }
        for (std::size_t i = 0; i < result.snapshot_->rows.size(); ++i) {
            const auto& row = result.snapshot_->rows[i];
            if (row.key.empty() || !append(row.key, 1024) || !std::isfinite(row.height) || row.height < 18 || row.height > 8192 ||
                row.cells.size() != result.snapshot_->columns.size() || !result.rows_.emplace(row.key, i).second)
                return error(TableErrorCode::invalid, "Invalid table row");
            for (const auto& cell : row.cells)
                if (!append(cell.text, 65536) || cell.value_type.empty() || !append(cell.value_type, 128))
                    return error(TableErrorCode::invalid, "Invalid table cell");
        }
        return result;
    }
    [[nodiscard]] const TableSnapshot& snapshot() const noexcept { return *snapshot_; }
    [[nodiscard]] std::optional<std::size_t> row(std::string_view key) const {
        const auto found = rows_.find(key); return found == rows_.end() ? std::nullopt : std::optional(found->second);
    }
    [[nodiscard]] std::optional<std::size_t> column(std::string_view key) const {
        const auto found = columns_.find(key); return found == columns_.end() ? std::nullopt : std::optional(found->second);
    }
private:
    [[nodiscard]] static std::unexpected<TableError> error(TableErrorCode code, std::string message) {
        return std::unexpected(TableError{code, std::move(message)});
    }
    std::shared_ptr<const TableSnapshot> snapshot_;
    std::unordered_map<std::string_view, std::size_t> rows_, columns_;
};

// Optional in-memory source/editor. The caller may provide a different editor
// with typed validation or persistence without coupling those policies to UI.
class TableMemory final : public TableSource, public TableEditor {
public:
    TableMemory(std::vector<TableColumn> columns = {}, std::vector<TableRow> rows = {}) {
        auto value = std::make_shared<TableSnapshot>(TableSnapshot{std::move(columns), std::move(rows), 0});
        if (auto valid = TableIndex::create(value); !valid) throw std::invalid_argument(valid.error().message);
        snapshot_ = std::move(value);
    }
    [[nodiscard]] TableResult<std::shared_ptr<const TableSnapshot>> snapshot() const override {
        std::scoped_lock lock(mutex_); return snapshot_;
    }
    [[nodiscard]] TableResult<std::uint64_t> replace(std::vector<TableColumn> columns, std::vector<TableRow> rows, std::uint64_t expected) {
        auto next = std::make_shared<TableSnapshot>(TableSnapshot{std::move(columns), std::move(rows), 0});
        if (auto valid = TableIndex::create(next); !valid) return std::unexpected(valid.error());
        std::scoped_lock lock(mutex_);
        if (snapshot_->revision != expected) return error(TableErrorCode::stale, "Table changed before replacement");
        if (snapshot_->columns == next->columns && snapshot_->rows == next->rows) return snapshot_->revision;
        if (snapshot_->revision == std::numeric_limits<std::uint64_t>::max()) return error(TableErrorCode::exhausted, "Table revision exhausted");
        next->revision = snapshot_->revision + 1; snapshot_ = std::move(next); return snapshot_->revision;
    }
    [[nodiscard]] TableResult<std::uint64_t> edit(const TableEdit& edit) override {
        if (edit.cells.size() > 4096) return error(TableErrorCode::invalid, "Table edit exceeds its cell limit");
        std::scoped_lock lock(mutex_);
        if (snapshot_->revision != edit.revision) return error(TableErrorCode::stale, "Table changed before the edit");
        auto index = TableIndex::create(snapshot_); if (!index) return std::unexpected(index.error());
        std::unordered_set<std::size_t> changed; changed.reserve(edit.cells.size()); bool different = false;
        for (const auto& input : edit.cells) {
            const auto row = index->row(input.row), column = index->column(input.column);
            if (!row || !column) return error(TableErrorCode::not_found, "Table cell was not found");
            const auto& existing = snapshot_->rows[*row].cells[*column]; const auto& col = snapshot_->columns[*column];
            if (!snapshot_->rows[*row].enabled || !col.enabled || !col.editable || !existing.enabled || !existing.editable ||
                !valid_accessibility_text(input.text) || !changed.insert(*row * snapshot_->columns.size() + *column).second)
                return error(TableErrorCode::invalid, "Invalid or duplicate table cell edit");
            different |= existing.text != input.text;
        }
        if (!different) return snapshot_->revision;
        if (snapshot_->revision == std::numeric_limits<std::uint64_t>::max()) return error(TableErrorCode::exhausted, "Table revision exhausted");
        auto next = std::make_shared<TableSnapshot>(*snapshot_);
        for (const auto& input : edit.cells) next->rows[*index->row(input.row)].cells[*index->column(input.column)].text = input.text;
        next->revision = snapshot_->revision + 1;
        if (auto valid = TableIndex::create(next); !valid) return std::unexpected(valid.error());
        snapshot_ = std::move(next); return snapshot_->revision;
    }
private:
    [[nodiscard]] static std::unexpected<TableError> error(TableErrorCode code, std::string message) {
        return std::unexpected(TableError{code, std::move(message)});
    }
    mutable std::mutex mutex_;
    std::shared_ptr<const TableSnapshot> snapshot_;
};

enum class TableSelectionMode { none, single, range };
enum class TableDirection { up, down, left, right, home, end };
struct TablePosition final {
    std::string row, column;
    friend bool operator==(const TablePosition&, const TablePosition&) = default;
};
struct TableSelection final {
    TablePosition anchor, current;
    friend bool operator==(const TableSelection&, const TableSelection&) = default;
};
struct TableRange final {
    std::size_t first_row = 0, last_row = 0, first_column = 0, last_column = 0; // Inclusive.
};

// Selection and navigation belong to a view and never submit document edits.
class TableController final {
public:
    explicit TableController(std::shared_ptr<const TableSource> source, TableSelectionMode mode = TableSelectionMode::range)
        : source_(std::move(source)), mode_(mode) {
        if (!source_ || unsigned(mode) > unsigned(TableSelectionMode::range)) throw std::invalid_argument("Invalid table controller");
    }
    [[nodiscard]] TableResult<bool> select(TablePosition position, bool extend = false) {
        if (auto ready = refresh(); !ready) return std::unexpected(ready.error());
        if (mode_ == TableSelectionMode::none) return error(TableErrorCode::invalid, "Table selection is disabled");
        if (!selectable(position)) return error(TableErrorCode::invalid, "Table position cannot be selected");
        TableSelection next{extend && mode_ == TableSelectionMode::range && selection_ ? selection_->anchor : position, std::move(position)};
        if (selection_ == next) return false;
        if (revision_ == std::numeric_limits<std::uint64_t>::max()) return error(TableErrorCode::exhausted, "Table view revision exhausted");
        selection_ = std::move(next); ++revision_; return true;
    }
    [[nodiscard]] TableResult<bool> navigate(TableDirection direction, bool extend = false) {
        if (auto ready = refresh(); !ready) return std::unexpected(ready.error());
        if (mode_ == TableSelectionMode::none || unsigned(direction) > unsigned(TableDirection::end))
            return error(TableErrorCode::invalid, "Table navigation is unavailable");
        const auto& data = index_->snapshot();
        if (data.rows.empty() || data.columns.empty()) return false;
        const auto valid = [&](std::size_t row, std::size_t column) {
            return data.rows[row].enabled && data.columns[column].enabled && data.rows[row].cells[column].enabled;
        };
        std::optional<std::pair<std::size_t, std::size_t>> target;
        if (!selection_) {
            const bool backwards = direction == TableDirection::up || direction == TableDirection::left || direction == TableDirection::end;
            for (std::size_t item = 0; item < data.rows.size() * data.columns.size(); ++item) {
                const auto offset = backwards ? data.rows.size() * data.columns.size() - item - 1 : item;
                const auto row = offset / data.columns.size(), column = offset % data.columns.size();
                if (valid(row, column)) { target = std::pair{row, column}; break; }
            }
        } else {
            auto row = std::ptrdiff_t(*index_->row(selection_->current.row));
            auto column = std::ptrdiff_t(*index_->column(selection_->current.column));
            std::ptrdiff_t dr = 0, dc = 0;
            if (direction == TableDirection::up) dr = -1; else if (direction == TableDirection::down) dr = 1;
            else if (direction == TableDirection::left) dc = -1; else if (direction == TableDirection::right) dc = 1;
            else if (direction == TableDirection::home) { column = -1; dc = 1; }
            else { column = std::ptrdiff_t(data.columns.size()); dc = -1; }
            for (row += dr, column += dc; row >= 0 && column >= 0 && row < std::ptrdiff_t(data.rows.size()) &&
                    column < std::ptrdiff_t(data.columns.size()); row += dr, column += dc) {
                if (valid(std::size_t(row), std::size_t(column))) { target = std::pair{std::size_t(row), std::size_t(column)}; break; }
            }
        }
        if (!target) return false;
        return select({data.rows[target->first].key, data.columns[target->second].key}, extend);
    }
    [[nodiscard]] TableResult<bool> clear_selection() {
        if (auto ready = refresh(); !ready) return std::unexpected(ready.error());
        if (!selection_) return false;
        if (revision_ == std::numeric_limits<std::uint64_t>::max()) return error(TableErrorCode::exhausted, "Table view revision exhausted");
        selection_.reset(); ++revision_; return true;
    }
    [[nodiscard]] TableResult<bool> set_mode(TableSelectionMode mode) {
        if (owner_ != std::this_thread::get_id()) return error(TableErrorCode::wrong_thread, "Table controller requires its owner thread");
        if (unsigned(mode) > unsigned(TableSelectionMode::range)) return error(TableErrorCode::invalid, "Invalid table selection mode");
        if (mode_ == mode) return false;
        if (revision_ == std::numeric_limits<std::uint64_t>::max()) return error(TableErrorCode::exhausted, "Table view revision exhausted");
        mode_ = mode; if (mode == TableSelectionMode::none) selection_.reset();
        else if (mode == TableSelectionMode::single && selection_) selection_->anchor = selection_->current;
        ++revision_; return true;
    }
    [[nodiscard]] TableResult<std::optional<TableSelection>> selection() {
        if (auto ready = refresh(); !ready) return std::unexpected(ready.error()); return selection_;
    }
    [[nodiscard]] TableResult<std::optional<TableRange>> range() {
        if (auto ready = refresh(); !ready) return std::unexpected(ready.error());
        if (!selection_) return std::optional<TableRange>{};
        const auto first_row = *index_->row(selection_->anchor.row), last_row = *index_->row(selection_->current.row);
        const auto first_column = *index_->column(selection_->anchor.column), last_column = *index_->column(selection_->current.column);
        return std::optional(TableRange{std::min(first_row, last_row), std::max(first_row, last_row),
                                       std::min(first_column, last_column), std::max(first_column, last_column)});
    }
    [[nodiscard]] TableResult<std::shared_ptr<const TableSnapshot>> source_snapshot() {
        if (auto ready = refresh(); !ready) return std::unexpected(ready.error()); return snapshot_;
    }
    [[nodiscard]] TableResult<std::uint64_t> view_revision() const {
        if (owner_ != std::this_thread::get_id()) return error(TableErrorCode::wrong_thread, "Table controller requires its owner thread");
        return revision_;
    }
private:
    [[nodiscard]] bool selectable(const TablePosition& position) const {
        const auto row = index_->row(position.row), column = index_->column(position.column); if (!row || !column) return false;
        return index_->snapshot().rows[*row].enabled && index_->snapshot().columns[*column].enabled && index_->snapshot().rows[*row].cells[*column].enabled;
    }
    [[nodiscard]] TableResult<void> refresh() {
        if (owner_ != std::this_thread::get_id()) return error(TableErrorCode::wrong_thread, "Table controller requires its owner thread");
        auto snapshot = source_->snapshot(); if (!snapshot) return std::unexpected(snapshot.error());
        if (index_ && snapshot_ == *snapshot) return {};
        auto next = TableIndex::create(*snapshot); if (!next) return std::unexpected(next.error());
        const auto valid = [&](const TablePosition& position) {
            const auto row = next->row(position.row), column = next->column(position.column); if (!row || !column) return false;
            return next->snapshot().rows[*row].enabled && next->snapshot().columns[*column].enabled && next->snapshot().rows[*row].cells[*column].enabled;
        };
        const bool pruned = selection_ && (!valid(selection_->anchor) || !valid(selection_->current));
        if (pruned && revision_ == std::numeric_limits<std::uint64_t>::max()) return error(TableErrorCode::exhausted, "Table view revision exhausted");
        if (pruned) { selection_.reset(); ++revision_; }
        index_ = std::move(*next); snapshot_ = std::move(*snapshot); return {};
    }
    [[nodiscard]] static std::unexpected<TableError> error(TableErrorCode code, std::string message) {
        return std::unexpected(TableError{code, std::move(message)});
    }
    std::shared_ptr<const TableSource> source_;
    std::shared_ptr<const TableSnapshot> snapshot_;
    std::optional<TableIndex> index_;
    std::optional<TableSelection> selection_;
    TableSelectionMode mode_;
    const std::thread::id owner_ = std::this_thread::get_id();
    std::uint64_t revision_ = 0;
};

} // namespace lvui
