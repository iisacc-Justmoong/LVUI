#pragma once

#include "CodeComposition.h"
#include "CodeEditorController.h"

#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace lvui {

// A view-local input session. Candidate composition is isolated from the
// source until a revision-checked commit succeeds.
class CodeEditorSession final {
public:
    [[nodiscard]] static CodeDocumentResult<CodeEditorSession> create(std::shared_ptr<const CodeDocumentSource> source,
            std::shared_ptr<CodeDocumentEditor> editor = {}) {
        const bool editable = bool(editor);
        auto controller = CodeEditorController::create(std::move(source), std::move(editor));
        if (!controller) return std::unexpected(controller.error());
        return CodeEditorSession(std::move(*controller), editable);
    }
    [[nodiscard]] CodeDocumentResult<std::shared_ptr<const CodeDocumentView>> view() const {
        if (auto ready = thread(); !ready) return std::unexpected(ready.error());
        if (composition_) return composition_->view(); return controller_.snapshot();
    }
    [[nodiscard]] CodeDocumentResult<CodeSelection> selection() const {
        if (auto ready = thread(); !ready) return std::unexpected(ready.error());
        if (composition_) { const auto range = composition_->selection(); return CodeSelection{range.location, range.location + range.length}; }
        return controller_.selection();
    }
    [[nodiscard]] CodeDocumentResult<std::optional<CodeDocumentRange>> marked() const {
        if (auto ready = thread(); !ready) return std::unexpected(ready.error());
        if (composition_) return std::optional<CodeDocumentRange>{composition_->marked()}; return std::optional<CodeDocumentRange>{};
    }
    [[nodiscard]] bool editable() const noexcept { return editable_; }
    [[nodiscard]] CodeDocumentResult<bool> set_selection(std::size_t anchor, std::size_t caret) {
        if (auto ready = ordinary(); !ready) return std::unexpected(ready.error()); return controller_.set_selection(anchor, caret);
    }
    [[nodiscard]] CodeDocumentResult<bool> move(CodeMove direction, bool extend = false) {
        if (auto ready = ordinary(); !ready) return std::unexpected(ready.error()); return controller_.move(direction, extend);
    }
    [[nodiscard]] CodeDocumentResult<bool> refresh() {
        if (auto ready = ordinary(); !ready) return std::unexpected(ready.error()); return controller_.refresh();
    }
    [[nodiscard]] CodeDocumentResult<std::optional<CodeDocumentChange>> erase(CodeErase direction) {
        if (auto ready = ordinary(); !ready) return std::unexpected(ready.error());
        busy_ = true; struct Reset final { bool& busy; ~Reset() { busy = false; } } reset{busy_};
        return controller_.erase(direction);
    }
    [[nodiscard]] CodeDocumentResult<bool> set_composition(std::string text,
            std::optional<CodeDocumentRange> replacement = {}, std::optional<CodeDocumentRange> selected = {}) {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        if (!editable_) return fail(CodeDocumentErrorCode::invalid, "Code editor is read-only");
        if (text.empty()) return cancel_composition();
        if (text.size() > CodeComposition::maximum_bytes) return fail(CodeDocumentErrorCode::too_large, "Composition exceeds 64 KiB");
        if (!code_document_detail::valid_text(text)) return fail(CodeDocumentErrorCode::invalid, "Composition requires NUL-free UTF-8");
        std::size_t units = 0;
        for (std::size_t at = 0; at < text.size();) {
            const auto bytes = code_document_detail::scalar_bytes(static_cast<unsigned char>(text[at]));
            units += bytes == 4 ? 2 : 1; at += bytes;
        }
        const auto selection = selected.value_or(CodeDocumentRange{units, 0});
        if (selection.location > units || selection.length > units - selection.location)
            return fail(CodeDocumentErrorCode::out_of_range, "Composition selection exceeds its text");
        auto base = controller_.snapshot(); if (!base) return std::unexpected(base.error());
        CodeDocumentRange original;
        if (composition_) {
            if (replacement && !same(*replacement, composition_->marked()))
                return fail(CodeDocumentErrorCode::invalid, "Composition replacement must cover its marked range");
            original = composition_->base_range();
            const auto current = composition_->selection();
            if (composition_->text() == text && current.location == original.location + selection.location && current.length == selection.length)
                return false;
        } else {
            auto current = controller_.selection(); if (!current) return std::unexpected(current.error());
            original = replacement.value_or(current->range());
        }
        auto next = CodeComposition::create(std::move(*base), original, std::move(text), selection);
        if (!next) return std::unexpected(next.error()); composition_ = std::move(*next); return true;
    }
    [[nodiscard]] CodeDocumentResult<bool> cancel_composition() {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        if (!composition_) return false; composition_.reset(); return true;
    }
    [[nodiscard]] CodeDocumentResult<std::optional<CodeDocumentChange>> commit_composition() {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        if (!composition_) return std::optional<CodeDocumentChange>{};
        busy_ = true; struct Reset final { bool& busy; ~Reset() { busy = false; } } reset{busy_};
        auto changed = controller_.replace_range(composition_->base_range(), composition_->text());
        if (!changed) return std::unexpected(changed.error());
        composition_.reset(); return std::optional<CodeDocumentChange>{*changed};
    }
    [[nodiscard]] CodeDocumentResult<CodeDocumentChange> insert(std::string text, std::optional<CodeDocumentRange> replacement = {}) {
        if (auto ready = mutable_state(); !ready) return std::unexpected(ready.error());
        if (!editable_) return fail(CodeDocumentErrorCode::invalid, "Code editor is read-only");
        CodeDocumentRange range;
        if (composition_) {
            if (replacement && !same(*replacement, composition_->marked()))
                return fail(CodeDocumentErrorCode::invalid, "Text replacement must cover the marked composition");
            range = composition_->base_range();
        } else {
            auto current = controller_.selection(); if (!current) return std::unexpected(current.error());
            range = replacement.value_or(current->range());
        }
        busy_ = true; struct Reset final { bool& busy; ~Reset() { busy = false; } } reset{busy_};
        auto changed = controller_.replace_range(range, std::move(text));
        if (!changed) return std::unexpected(changed.error()); composition_.reset(); return *changed;
    }
private:
    CodeEditorSession(CodeEditorController controller, bool editable) : controller_(std::move(controller)), editable_(editable) {}
    [[nodiscard]] static std::unexpected<CodeDocumentError> fail(CodeDocumentErrorCode code, std::string message) {
        return std::unexpected(CodeDocumentError{code, std::move(message)});
    }
    [[nodiscard]] static bool same(CodeDocumentRange a, CodeDocumentRange b) noexcept { return a.location == b.location && a.length == b.length; }
    [[nodiscard]] CodeDocumentResult<void> thread() const {
        if (owner_ != std::this_thread::get_id()) return fail(CodeDocumentErrorCode::wrong_thread, "Code input session requires its owner thread"); return {};
    }
    [[nodiscard]] CodeDocumentResult<void> mutable_state() const {
        if (auto ready = thread(); !ready) return ready;
        if (busy_) return fail(CodeDocumentErrorCode::busy, "A code input edit is committing"); return {};
    }
    [[nodiscard]] CodeDocumentResult<void> ordinary() const {
        if (auto ready = mutable_state(); !ready) return ready;
        if (composition_) return fail(CodeDocumentErrorCode::busy, "Commit or cancel the active composition first"); return {};
    }
    CodeEditorController controller_;
    std::optional<CodeComposition> composition_;
    const bool editable_;
    const std::thread::id owner_ = std::this_thread::get_id();
    bool busy_ = false;
};

} // namespace lvui
