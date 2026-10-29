#pragma once

#include "Table.h"

#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace lvui {

struct TableEditState final {
    TablePosition position;
    std::string original, draft, value_type;
    std::uint64_t source_revision = 0;
};

// One edit owns a draft, not the document. The injected editor is the sole
// mutation boundary and must atomically honor the supplied source revision.
class TableEditing final {
public:
    [[nodiscard]] static TableResult<TableEditing> begin(std::shared_ptr<const TableSnapshot> snapshot,
            std::shared_ptr<TableEditor> editor, TablePosition position) {
        if (!editor) return error(TableErrorCode::invalid, "Table editor is required");
        auto index = TableIndex::create(std::move(snapshot)); if (!index) return std::unexpected(index.error());
        const auto row = index->row(position.row), column = index->column(position.column);
        if (!row || !column) return error(TableErrorCode::not_found, "Table edit cell was not found");
        const auto& data = index->snapshot(); const auto& cell = data.rows[*row].cells[*column];
        if (!data.rows[*row].enabled || !data.columns[*column].enabled || !data.columns[*column].editable || !cell.enabled || !cell.editable)
            return error(TableErrorCode::invalid, "Table cell cannot be edited");
        return TableEditing(std::move(editor), {std::move(position), cell.text, cell.text, cell.value_type, data.revision});
    }
    [[nodiscard]] TableResult<TableEditState> state() const {
        if (owner_ != std::this_thread::get_id()) return error(TableErrorCode::wrong_thread, "Table edit requires its owner thread");
        if (!active_) return error(TableErrorCode::invalid, "Table edit has ended"); return state_;
    }
    [[nodiscard]] TableResult<bool> set_draft(std::string value) {
        if (owner_ != std::this_thread::get_id()) return error(TableErrorCode::wrong_thread, "Table edit requires its owner thread");
        if (committing_) return error(TableErrorCode::busy, "Table edit is committing");
        if (!active_ || !valid_accessibility_text(value)) return error(TableErrorCode::invalid, "Invalid table edit draft");
        if (state_.draft == value) return false; state_.draft = std::move(value); return true;
    }
    [[nodiscard]] TableResult<std::uint64_t> commit() {
        if (owner_ != std::this_thread::get_id()) return error(TableErrorCode::wrong_thread, "Table edit requires its owner thread");
        if (!active_) return error(TableErrorCode::invalid, "Table edit has ended");
        if (committing_) return error(TableErrorCode::busy, "Table edit is committing");
        committing_ = true; struct Reset final { bool& flag; ~Reset() { flag = false; } } reset{committing_};
        auto result = editor_->edit({.revision=state_.source_revision,
            .cells={{state_.position.row, state_.position.column, state_.draft}}});
        if (!result) return result; active_ = false; return result;
    }
    [[nodiscard]] TableResult<void> cancel() {
        if (owner_ != std::this_thread::get_id()) return error(TableErrorCode::wrong_thread, "Table edit requires its owner thread");
        if (!active_) return error(TableErrorCode::invalid, "Table edit has ended");
        if (committing_) return error(TableErrorCode::busy, "Table edit is committing"); active_ = false; return {};
    }
private:
    TableEditing(std::shared_ptr<TableEditor> editor, TableEditState state) : editor_(std::move(editor)), state_(std::move(state)) {}
    [[nodiscard]] static std::unexpected<TableError> error(TableErrorCode code, std::string message) {
        return std::unexpected(TableError{code, std::move(message)});
    }
    std::shared_ptr<TableEditor> editor_;
    TableEditState state_;
    const std::thread::id owner_ = std::this_thread::get_id();
    bool active_ = true, committing_ = false;
};

} // namespace lvui
