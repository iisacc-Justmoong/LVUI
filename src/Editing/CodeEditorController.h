#pragma once

#include <LVUI/CodeViewport.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace lvui {

struct CodeSelection final {
    std::size_t anchor = 0, caret = 0;
    [[nodiscard]] CodeDocumentRange range() const noexcept {
        return {std::min(anchor, caret), std::max(anchor, caret) - std::min(anchor, caret)};
    }
    friend bool operator==(const CodeSelection&, const CodeSelection&) = default;
};
enum class CodeMove { previous_scalar, next_scalar, document_start, document_end, line_start, line_end, previous_line, next_line };
enum class CodeErase { previous_scalar, next_scalar };

// Selection belongs to a view. Document edits are delegated to an injected
// editor with the snapshot revision observed before the operation.
class CodeEditorController final {
public:
    [[nodiscard]] static CodeDocumentResult<CodeEditorController> create(
            std::shared_ptr<const CodeDocumentSource> source, std::shared_ptr<CodeDocumentEditor> editor = {}) {
        if (!source) return fail(CodeDocumentErrorCode::invalid, "Code editor source is required");
        auto snapshot = source->snapshot(); if (!snapshot) return std::unexpected(snapshot.error());
        if (!*snapshot) return fail(CodeDocumentErrorCode::invalid, "Code editor source returned no snapshot");
        auto zero = (*snapshot)->read({0, 0}, 0); if (!zero) return std::unexpected(zero.error());
        return CodeEditorController(std::move(source), std::move(editor), std::move(*snapshot));
    }
    [[nodiscard]] CodeDocumentResult<std::shared_ptr<const CodeDocumentView>> snapshot() const {
        if (auto ready = thread(); !ready) return std::unexpected(ready.error()); return view_;
    }
    [[nodiscard]] CodeDocumentResult<CodeSelection> selection() const {
        if (auto ready = thread(); !ready) return std::unexpected(ready.error()); return selection_;
    }
    [[nodiscard]] CodeDocumentResult<std::uint64_t> view_revision() const {
        if (auto ready = thread(); !ready) return std::unexpected(ready.error()); return view_revision_;
    }
    [[nodiscard]] CodeDocumentResult<bool> refresh() {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        auto latest = source_->snapshot(); if (!latest) return std::unexpected(latest.error());
        if (!*latest || (*latest)->revision() < view_->revision()) return fail(CodeDocumentErrorCode::invalid, "Code source revision moved backwards");
        if ((*latest)->revision() == view_->revision()) return false;
        const auto normalize = [&](std::size_t position) -> CodeDocumentResult<std::size_t> {
            position = std::min(position, (*latest)->length());
            auto valid = (*latest)->read({position, 0}, 0);
            if (valid) return position;
            if (valid.error().code != CodeDocumentErrorCode::out_of_range || !position) return std::unexpected(valid.error());
            --position; valid = (*latest)->read({position, 0}, 0); if (!valid) return std::unexpected(valid.error()); return position;
        };
        auto anchor = normalize(selection_.anchor); if (!anchor) return std::unexpected(anchor.error());
        auto caret = normalize(selection_.caret); if (!caret) return std::unexpected(caret.error());
        const CodeSelection next{*anchor, *caret};
        if (next != selection_ && view_revision_ == std::numeric_limits<std::uint64_t>::max())
            return fail(CodeDocumentErrorCode::exhausted, "Code view revision exhausted");
        view_ = std::move(*latest); preferred_column_.reset(); if (next != selection_) { selection_ = next; ++view_revision_; } return true;
    }
    [[nodiscard]] CodeDocumentResult<bool> set_selection(std::size_t anchor, std::size_t caret) {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        auto a = view_->read({anchor, 0}, 0); if (!a) return std::unexpected(a.error());
        auto c = view_->read({caret, 0}, 0); if (!c) return std::unexpected(c.error());
        preferred_column_.reset(); const CodeSelection next{anchor, caret}; if (next == selection_) return false;
        if (view_revision_ == std::numeric_limits<std::uint64_t>::max()) return fail(CodeDocumentErrorCode::exhausted, "Code view revision exhausted");
        selection_ = next; ++view_revision_; return true;
    }
    [[nodiscard]] CodeDocumentResult<bool> move(CodeMove direction, bool extend = false) {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        if (unsigned(direction) > unsigned(CodeMove::next_line)) return fail(CodeDocumentErrorCode::invalid, "Invalid code navigation direction");
        const auto selected = selection_.range(); std::size_t target = selection_.caret;
        std::optional<std::size_t> preferred;
        if (direction == CodeMove::document_start) target = 0;
        else if (direction == CodeMove::document_end) target = view_->length();
        else if (direction == CodeMove::line_start || direction == CodeMove::line_end ||
                 direction == CodeMove::previous_line || direction == CodeMove::next_line) {
            auto line = code_line_at(*view_, target); if (!line) return std::unexpected(line.error());
            auto bounds = code_line_bounds(*view_, *line); if (!bounds) return std::unexpected(bounds.error());
            if (direction == CodeMove::line_start) target = bounds->start;
            else if (direction == CodeMove::line_end) target = bounds->end;
            else {
                preferred = preferred_column_.value_or(std::min(target, bounds->end) - bounds->start);
                const auto destination = direction == CodeMove::previous_line ? (*line ? *line - 1 : *line) :
                    (*line + 1 < view_->line_count() ? *line + 1 : *line);
                auto other = code_line_bounds(*view_, destination); if (!other) return std::unexpected(other.error());
                target = other->start + std::min(*preferred, other->end - other->start);
                auto valid = view_->read({target, 0}, 0);
                if (!valid && valid.error().code == CodeDocumentErrorCode::out_of_range && target > other->start) {
                    --target; valid = view_->read({target, 0}, 0);
                }
                if (!valid) return std::unexpected(valid.error());
            }
        }
        else if (!extend && selected.length) target = direction == CodeMove::previous_scalar ? selected.location : selected.location + selected.length;
        else if (direction == CodeMove::previous_scalar && target) {
            --target; auto valid = view_->read({target, 0}, 0);
            if (!valid && valid.error().code == CodeDocumentErrorCode::out_of_range && target) { --target; valid = view_->read({target, 0}, 0); }
            if (!valid) return std::unexpected(valid.error());
        } else if (direction == CodeMove::next_scalar && target < view_->length()) {
            ++target; auto valid = view_->read({target, 0}, 0);
            if (!valid && valid.error().code == CodeDocumentErrorCode::out_of_range && target < view_->length()) { ++target; valid = view_->read({target, 0}, 0); }
            if (!valid) return std::unexpected(valid.error());
        }
        auto moved = set_selection(extend ? selection_.anchor : target, target);
        if (moved && preferred) preferred_column_ = preferred;
        return moved;
    }
    [[nodiscard]] CodeDocumentResult<CodeDocumentChange> replace_selection(std::string text) {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        return replace_range(selection_.range(), std::move(text));
    }
    // An explicit range is used by platform text services without first moving
    // the visible selection, so a failed or stale edit leaves it unchanged.
    [[nodiscard]] CodeDocumentResult<CodeDocumentChange> replace_range(CodeDocumentRange range, std::string text) {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        if (!editor_) return fail(CodeDocumentErrorCode::invalid, "Code editor is read-only");
        const auto before = view_;
        if (range.location > before->length() || range.length > before->length() - range.location)
            return fail(CodeDocumentErrorCode::out_of_range, "Code edit range exceeds the document");
        auto first = before->read({range.location, 0}, 0); if (!first) return std::unexpected(first.error());
        auto last = before->read({range.location + range.length, 0}, 0); if (!last) return std::unexpected(last.error());
        if (view_revision_ == std::numeric_limits<std::uint64_t>::max()) return fail(CodeDocumentErrorCode::exhausted, "Code view revision exhausted");
        busy_ = true; struct Reset final { bool& busy; ~Reset() { busy = false; } } reset{busy_};
        auto changed = editor_->replace({.revision=before->revision(), .range=range, .text=std::move(text)});
        if (!changed) return std::unexpected(changed.error());
        if (changed->before_revision != before->revision() || changed->revision < before->revision() ||
            changed->removed.location != range.location || changed->removed.length != range.length)
            return fail(CodeDocumentErrorCode::invalid, "Code editor returned an inconsistent change");
        auto latest = source_->snapshot(); if (!latest) return std::unexpected(latest.error());
        if (!*latest || (*latest)->revision() != changed->revision || range.location > (*latest)->length() ||
            changed->inserted_length > (*latest)->length() - range.location ||
            before->length() - range.length != (*latest)->length() - changed->inserted_length)
            return fail(CodeDocumentErrorCode::invalid, "Code source did not publish the committed change");
        const auto caret = range.location + changed->inserted_length;
        auto boundary = (*latest)->read({caret, 0}, 0); if (!boundary) return std::unexpected(boundary.error());
        const CodeSelection next{caret, caret}; view_ = std::move(*latest); preferred_column_.reset();
        if (next != selection_) { selection_ = next; ++view_revision_; }
        return *changed;
    }
    [[nodiscard]] CodeDocumentResult<std::optional<CodeDocumentChange>> erase(CodeErase direction) {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        if (!editor_) return fail(CodeDocumentErrorCode::invalid, "Code editor is read-only");
        if (unsigned(direction) > unsigned(CodeErase::next_scalar)) return fail(CodeDocumentErrorCode::invalid, "Invalid code deletion direction");
        auto range = selection_.range();
        if (!range.length) {
            auto boundary = selection_.caret;
            if ((direction == CodeErase::previous_scalar && !boundary) ||
                (direction == CodeErase::next_scalar && boundary == view_->length())) return std::optional<CodeDocumentChange>{};
            if (direction == CodeErase::previous_scalar) --boundary; else ++boundary;
            auto valid = view_->read({boundary, 0}, 0);
            if (!valid && valid.error().code == CodeDocumentErrorCode::out_of_range) {
                if (direction == CodeErase::previous_scalar && boundary) --boundary;
                else if (direction == CodeErase::next_scalar && boundary < view_->length()) ++boundary;
                valid = view_->read({boundary, 0}, 0);
            }
            if (!valid) return std::unexpected(valid.error());
            range = {std::min(boundary, selection_.caret), std::max(boundary, selection_.caret) - std::min(boundary, selection_.caret)};
        }
        auto changed = replace_range(range, {}); if (!changed) return std::unexpected(changed.error());
        return std::optional<CodeDocumentChange>{*changed};
    }
private:
    CodeEditorController(std::shared_ptr<const CodeDocumentSource> source, std::shared_ptr<CodeDocumentEditor> editor,
                         std::shared_ptr<const CodeDocumentView> view)
        : source_(std::move(source)), editor_(std::move(editor)), view_(std::move(view)) {}
    [[nodiscard]] static std::unexpected<CodeDocumentError> fail(CodeDocumentErrorCode code, std::string message) {
        return std::unexpected(CodeDocumentError{code, std::move(message)});
    }
    [[nodiscard]] CodeDocumentResult<void> thread() const {
        if (owner_ != std::this_thread::get_id()) return fail(CodeDocumentErrorCode::wrong_thread, "Code editor requires its owner thread"); return {};
    }
    [[nodiscard]] CodeDocumentResult<void> mutable_state() const {
        if (auto ready = thread(); !ready) return ready;
        if (busy_) return fail(CodeDocumentErrorCode::busy, "A code edit is committing"); return {};
    }
    std::shared_ptr<const CodeDocumentSource> source_;
    std::shared_ptr<CodeDocumentEditor> editor_;
    std::shared_ptr<const CodeDocumentView> view_;
    CodeSelection selection_;
    std::uint64_t view_revision_ = 0;
    std::optional<std::size_t> preferred_column_;
    const std::thread::id owner_ = std::this_thread::get_id();
    bool busy_ = false;
};

} // namespace lvui
