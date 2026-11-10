#include <LVUI/NativeTextBoundary.h>
#include <iiSDKCore/Core/Result/Result.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Index {
    std::string text;
    static ii::core::Result<Index> create(std::string value) {
        if (value == "bad") return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Bad text");
        if (value == "throw") throw std::runtime_error("Index failed");
        return Index{std::move(value)};
    }
    std::size_t utf16_size() const noexcept { return text.size(); }
    ii::core::Result<std::size_t> byte_for_utf16(std::size_t value) const {
        if (value > text.size()) return ii::core::fail(ii::core::ErrorCode::out_of_range, "Bad offset");
        return value;
    }
    std::size_t previous_grapheme(std::size_t value) const noexcept { return value ? value - 1 : 0; }
    std::size_t next_grapheme(std::size_t value) const noexcept { return (std::min)(value + 1, text.size()); }
    std::size_t previous_word(std::size_t value) const noexcept {
        while (value && text[value - 1] != ' ') --value;
        return value;
    }
    std::size_t next_word(std::size_t value) const noexcept {
        while (value < text.size() && text[value] != ' ') ++value;
        return value;
    }
    bool word_at(std::size_t value) const noexcept { return value < text.size() && text[value] != ' '; }
};
}
int main() {
    auto result = lvui::native::native_text_boundary<Index>("abc def");
    require(bool(result), "Injected text index was rejected");
    const auto& boundary = **result;
    require(boundary.length() == 7 && boundary.byte_offset(3) == 3 && !boundary.byte_offset(8), "Text index offsets changed");
    require(boundary.previous_grapheme(2) == 1 && boundary.next_grapheme(2) == 3, "Text index grapheme delegation changed");
    auto word = boundary.word_range(1); require(word && word->location == 0 && word->length == 3, "Word range changed");
    auto separator = boundary.word_range(3); require(separator && separator->location == 3 && separator->length == 1, "Separator range changed");
    require(!boundary.word_range(8), "Out-of-range word position was accepted");
    auto bad = lvui::native::native_text_boundary<Index>("bad");
    auto thrown = lvui::native::native_text_boundary<Index>("throw");
    require(!bad && bad.error().message == "Bad text" && !thrown, "Index failures were not converted");
}
