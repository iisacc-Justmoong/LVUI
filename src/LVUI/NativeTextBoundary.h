#pragma once
#include "TextInput.h"
#include <iiGUI/TextBoundary.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace lvui::native {
// Bridges generic Unicode indexing to LVUI's editing interface. It never
// creates a widget and owns no editing, selection, or composition state.
class NativeTextBoundary final : public TextInputBoundary {
    ii::gui::TextIndex index_;
public:
    explicit NativeTextBoundary(ii::gui::TextIndex value) : index_(std::move(value)) {}
    [[nodiscard]] std::size_t length() const noexcept override { return index_.utf16_size(); }
    [[nodiscard]] std::optional<std::size_t> byte_offset(std::size_t unit) const override {
        auto value = index_.byte_for_utf16(unit);
        return value ? std::optional<std::size_t>{*value} : std::nullopt;
    }
    [[nodiscard]] std::size_t previous_grapheme(std::size_t value) const noexcept override { return index_.previous_grapheme(value); }
    [[nodiscard]] std::size_t next_grapheme(std::size_t value) const noexcept override { return index_.next_grapheme(value); }
    [[nodiscard]] std::size_t previous_word(std::size_t value) const noexcept override { return index_.previous_word(value); }
    [[nodiscard]] std::size_t next_word(std::size_t value) const noexcept override { return index_.next_word(value); }
};
[[nodiscard]] inline TextInputResult<std::shared_ptr<const TextInputBoundary>> native_text_boundary(std::string_view text) {
    try {
        auto index = ii::gui::TextIndex::create(std::string(text));
        if (!index) return std::unexpected(TextInputError{TextInputErrorCode::unavailable,index.error().message});
        return std::shared_ptr<const TextInputBoundary>(std::make_shared<NativeTextBoundary>(std::move(*index)));
    } catch (...) {
        return std::unexpected(TextInputError{TextInputErrorCode::unavailable,"Cannot create native Unicode boundary"});
    }
}
} // namespace lvui::native
