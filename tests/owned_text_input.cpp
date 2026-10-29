#include <LVUI/TextInput.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::TextInputResult<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}

// A small deterministic boundary service for the model tests. It is deliberately
// separate from production Unicode segmentation; the host supplies that service.
class Boundary final : public lvui::TextInputBoundary {
    std::vector<std::size_t> bytes_{0};
    std::vector<std::pair<std::size_t,std::size_t>> clusters_;
public:
    explicit Boundary(std::string_view text) {
        std::size_t byte = 0, unit = 0;
        while (byte < text.size()) {
            const auto lead = static_cast<unsigned char>(text[byte]);
            const std::size_t count = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
            char32_t scalar = lead < 0x80 ? lead : lead & ((1u << (7 - count)) - 1);
            for (std::size_t i = 1; i < count; ++i)
                scalar = (scalar << 6) | (static_cast<unsigned char>(text[byte + i]) & 0x3f);
            const auto first = unit;
            if (scalar > 0xffff) { bytes_.push_back(invalid); ++unit; }
            byte += count; ++unit; bytes_.push_back(byte);
            if (scalar == 0x301 && !clusters_.empty()) clusters_.back().second = unit;
            else clusters_.push_back({first,unit});
        }
    }
    static constexpr std::size_t invalid = std::numeric_limits<std::size_t>::max();
    std::size_t length() const noexcept override { return bytes_.size() - 1; }
    std::optional<std::size_t> byte_offset(std::size_t unit) const override {
        if (unit >= bytes_.size() || bytes_[unit] == invalid) return std::nullopt;
        return bytes_[unit];
    }
    std::size_t previous_grapheme(std::size_t unit) const noexcept override {
        for (auto it = clusters_.rbegin(); it != clusters_.rend(); ++it)
            if (it->first < unit) return it->first;
        return 0;
    }
    std::size_t next_grapheme(std::size_t unit) const noexcept override {
        for (auto [first,last] : clusters_) if (last > unit) return last;
        return length();
    }
    std::size_t previous_word(std::size_t unit) const noexcept override { return previous_grapheme(unit); }
    std::size_t next_word(std::size_t unit) const noexcept override { return next_grapheme(unit); }
};

lvui::TextInputResult<std::shared_ptr<const lvui::TextInputBoundary>> index(std::string_view text) {
    return std::shared_ptr<const lvui::TextInputBoundary>(std::make_shared<Boundary>(text));
}

void unicode_and_ranges() {
    auto text = take(lvui::TextInput::create(index, "A한😀e\xcc\x81Z"));
    require(text.length() == 7 && text.caret() == 7, "UTF-16 length or initial caret is wrong");
    require(!text.set_selection(3,3) && text.caret() == 7, "Selection split a surrogate pair");
    take(text.set_selection(2,4));
    require(take(text.read(text.selection())) == "😀", "UTF-16 selection read split text");
    require(!text.read({std::numeric_limits<std::size_t>::max(),1}), "Overflowing range was accepted");
    take(text.set_selection(7,7));
    take(text.move(-1,false));
    require(text.caret() == 6, "Caret did not move across the last character");
    take(text.move(-1,false));
    require(text.caret() == 4, "Caret split a combining grapheme");
    require(take(text.erase(-1)) && text.text() == "A한e\xcc\x81Z", "Backspace did not erase an entire emoji");
    take(text.set_selection(2,4));
    take(text.move(-1,false));
    require(text.caret() == 2 && text.anchor() == 2, "Arrow movement did not collapse selection");
    take(text.move(1,true));
    require(text.selection() == lvui::TextInputRange{2,2}, "Shift selection split a grapheme");
}

void composition() {
    auto text = take(lvui::TextInput::create(index, "A😀Z"));
    take(text.set_selection(1,3));
    take(text.set_composition("ㅎ"));
    require(text.text() == "AㅎZ" && text.marked() == lvui::TextInputRange{1,1} && text.caret() == 2,
            "Initial Korean composition did not replace the selection");
    take(text.set_composition("하"));
    take(text.set_composition("한"));
    require(text.text() == "A한Z" && text.marked() == lvui::TextInputRange{1,1},
            "Composition update duplicated marked text");
    const auto before = text.text();
    require(!text.set_composition("😀",std::nullopt,lvui::TextInputRange{1,0}) && text.text() == before,
            "Invalid composition selection changed text");
    require(take(text.cancel_composition()) && text.text() == "A😀Z" && text.selection() == lvui::TextInputRange{1,2},
            "Composition cancellation failed to restore the original selection and text");
    take(text.set_composition("하"));
    require(take(text.insert("한")) && text.text() == "A한Z" && !text.marked(),
            "Committing text did not replace marked text");
    take(text.set_composition("글"));
    require(take(text.commit_composition()) && text.text() == "A한글Z" && !text.marked(),
            "Unmarking failed to commit the composition");
    require(!take(text.commit_composition()), "Committing absent marked text reported a change");
}

void transactions() {
    auto text = take(lvui::TextInput::create(index,"safe"));
    take(text.set_selection(1,3));
    const auto selection = text.selection();
    require(!text.insert(std::string("\xff",1)) && text.text() == "safe" && text.selection() == selection,
            "Invalid UTF-8 changed the document");
    require(!text.insert(std::string(lvui::TextInput::maximum_bytes + 1,'x')) && text.text() == "safe",
            "Oversized text changed the document");
    require(!text.insert("x",lvui::TextInputRange{100,1}) && text.text() == "safe",
            "Invalid replacement range changed the document");
    require(take(text.insert("OK")) && text.text() == "sOKe" && text.caret() == 3,
            "Selection replacement failed");
    require(!take(text.set_text("sOKe")) && text.caret() == 3, "Unchanged program text disturbed selection");
    take(text.set_text("new"));
    require(text.text() == "new" && text.caret() == 3, "Program text did not reset the caret");
    auto unavailable=[](std::string_view)->lvui::TextInputResult<std::shared_ptr<const lvui::TextInputBoundary>> {
        return std::unexpected(lvui::TextInputError{lvui::TextInputErrorCode::unavailable,"missing boundary service"});
    };
    require(!lvui::TextInput::create(unavailable,"x"), "A failed boundary service produced a field");
}
}

int main() { unicode_and_ranges(); composition(); transactions(); }
