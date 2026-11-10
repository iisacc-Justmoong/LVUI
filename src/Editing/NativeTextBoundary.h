#pragma once
#include <LVUI/TextInput.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace lvui::native {
// Bridges generic Unicode indexing to LVUI's editing interface. It never
// creates a widget and owns no editing, selection, or composition state.
template<class Index>
class NativeTextBoundary final : public TextInputBoundary {
    Index index_;
public:
    explicit NativeTextBoundary(Index value) : index_(std::move(value)) {}
    [[nodiscard]] std::size_t length() const noexcept override { return index_.utf16_size(); }
    [[nodiscard]] std::optional<std::size_t> byte_offset(std::size_t unit) const override {
        auto value = index_.byte_for_utf16(unit);
        return value ? std::optional<std::size_t>{*value} : std::nullopt;
    }
    [[nodiscard]] std::size_t previous_grapheme(std::size_t value) const noexcept override { return index_.previous_grapheme(value); }
    [[nodiscard]] std::size_t next_grapheme(std::size_t value) const noexcept override { return index_.next_grapheme(value); }
    [[nodiscard]] std::size_t previous_word(std::size_t value) const noexcept override { return index_.previous_word(value); }
    [[nodiscard]] std::size_t next_word(std::size_t value) const noexcept override { return index_.next_word(value); }
    [[nodiscard]] std::optional<TextInputRange> word_range(std::size_t position) const noexcept override {
        const auto length=index_.utf16_size();
        if(position>length)return std::nullopt;
        if(!length)return TextInputRange{};
        auto at=position==length?index_.previous_grapheme(position):position;
        if(at&&index_.next_grapheme(index_.previous_grapheme(at))>at)at=index_.previous_grapheme(at);
        if(!index_.word_at(at))return TextInputRange{at,index_.next_grapheme(at)-at};
        const auto first=index_.previous_word(index_.next_grapheme(at));
        return TextInputRange{first,index_.next_word(at)-first};
    }
};
template<class Index>
[[nodiscard]] inline TextInputResult<std::shared_ptr<const TextInputBoundary>> native_text_boundary(std::string_view text) {
    try {
        auto index = Index::create(std::string(text));
        if (!index) return std::unexpected(TextInputError{TextInputErrorCode::unavailable,index.error().message});
        return std::shared_ptr<const TextInputBoundary>(std::make_shared<NativeTextBoundary<Index>>(std::move(*index)));
    } catch (...) {
        return std::unexpected(TextInputError{TextInputErrorCode::unavailable,"Cannot create native Unicode boundary"});
    }
}
} // namespace lvui::native
