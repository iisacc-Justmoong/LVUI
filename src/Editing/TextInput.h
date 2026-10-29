#pragma once
#include <LVUI/Text.h>
#include <algorithm>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace lvui {
struct TextInputRange final {
    std::size_t location = 0, length = 0;
    friend bool operator==(const TextInputRange&, const TextInputRange&) = default;
};
enum class TextInputErrorCode { invalid_text, invalid_range, invalid_boundary, unavailable, too_large, composing };
struct TextInputError final { TextInputErrorCode code; std::string message; };
template<class T> using TextInputResult = std::expected<T,TextInputError>;

// A replaceable Unicode service. Indices and ranges use UTF-16 units, while
// byte_offset addresses UTF-8. Implementations retain their own immutable index.
class TextInputBoundary {
public:
    virtual ~TextInputBoundary() = default;
    [[nodiscard]] virtual std::size_t length() const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::size_t> byte_offset(std::size_t) const = 0;
    [[nodiscard]] virtual std::size_t previous_grapheme(std::size_t) const noexcept = 0;
    [[nodiscard]] virtual std::size_t next_grapheme(std::size_t) const noexcept = 0;
    [[nodiscard]] virtual std::size_t previous_word(std::size_t) const noexcept = 0;
    [[nodiscard]] virtual std::size_t next_word(std::size_t) const noexcept = 0;
};
using TextInputBoundaryFactory = std::function<TextInputResult<std::shared_ptr<const TextInputBoundary>>(std::string_view)>;

// Owns the editing transaction; neither the Unicode service nor a platform
// text-input host owns its text, selection or in-progress IME composition.
class TextInput final {
    struct Original final {
        std::string text;
        std::shared_ptr<const TextInputBoundary> boundary;
        std::size_t anchor = 0, caret = 0;
    };
    struct Replacement final {
        std::string text;
        std::shared_ptr<const TextInputBoundary> boundary;
        std::size_t inserted_length = 0;
    };
    TextInputBoundaryFactory factory_;
    std::string text_;
    std::shared_ptr<const TextInputBoundary> boundary_;
    std::size_t anchor_ = 0, caret_ = 0;
    std::optional<TextInputRange> marked_;
    std::optional<Original> original_;

    explicit TextInput(TextInputBoundaryFactory factory) : factory_(std::move(factory)) {}
    template<class T> [[nodiscard]] static TextInputResult<T> failure(TextInputErrorCode code, std::string message) {
        return std::unexpected(TextInputError{code,std::move(message)});
    }
    [[nodiscard]] TextInputResult<std::shared_ptr<const TextInputBoundary>> index(std::string_view text) const {
        if (text.size() > maximum_bytes)
            return failure<std::shared_ptr<const TextInputBoundary>>(TextInputErrorCode::too_large,"Text input exceeds 64 KiB");
        if (!valid_accessibility_text(text))
            return failure<std::shared_ptr<const TextInputBoundary>>(TextInputErrorCode::invalid_text,"Text input must be NUL-free UTF-8");
        if (!factory_)
            return failure<std::shared_ptr<const TextInputBoundary>>(TextInputErrorCode::unavailable,"Text boundary service is unavailable");
        auto value = factory_(text);
        if (!value) return std::unexpected(value.error());
        if (!*value || (*value)->length() > text.size() || (*value)->byte_offset(0) != std::optional<std::size_t>{0} ||
            (*value)->byte_offset((*value)->length()) != std::optional<std::size_t>{text.size()})
            return failure<std::shared_ptr<const TextInputBoundary>>(TextInputErrorCode::invalid_boundary,"Text boundary service returned an invalid index");
        return value;
    }
    [[nodiscard]] TextInputResult<std::pair<std::size_t,std::size_t>> bytes(TextInputRange range) const {
        if (range.location > length() || range.length > length() - range.location)
            return failure<std::pair<std::size_t,std::size_t>>(TextInputErrorCode::invalid_range,"Text range exceeds the document");
        const auto first = boundary_->byte_offset(range.location), last = boundary_->byte_offset(range.location + range.length);
        if (!first || !last || *first > *last || *last > text_.size())
            return failure<std::pair<std::size_t,std::size_t>>(TextInputErrorCode::invalid_range,"Text range splits a Unicode scalar");
        return std::pair{*first,*last};
    }
    [[nodiscard]] TextInputResult<Replacement> replace(TextInputRange range,std::string_view inserted) const {
        auto offsets = bytes(range); if (!offsets) return std::unexpected(offsets.error());
        auto inserted_index = index(inserted); if (!inserted_index) return std::unexpected(inserted_index.error());
        const auto retained = text_.size() - (offsets->second - offsets->first);
        if (inserted.size() > maximum_bytes - retained)
            return failure<Replacement>(TextInputErrorCode::too_large,"Text input exceeds 64 KiB");
        Replacement value;
        value.text.reserve(retained + inserted.size());
        value.text.append(text_,0,offsets->first);
        value.text.append(inserted);
        value.text.append(text_,offsets->second,std::string::npos);
        auto candidate = index(value.text); if (!candidate) return std::unexpected(candidate.error());
        value.inserted_length = (*inserted_index)->length();
        if ((*candidate)->length() != length() - range.length + value.inserted_length ||
            !(*candidate)->byte_offset(range.location + value.inserted_length))
            return failure<Replacement>(TextInputErrorCode::invalid_boundary,"Text boundary service returned an inconsistent replacement");
        value.boundary = std::move(*candidate);
        return value;
    }
    template<class T, class F> [[nodiscard]] static TextInputResult<T> protect(F&& operation) {
        try { return operation(); }
        catch (...) { return failure<T>(TextInputErrorCode::unavailable,"Text input operation failed"); }
    }
public:
    static constexpr std::size_t maximum_bytes = 64 * 1024;
    [[nodiscard]] static TextInputResult<TextInput> create(TextInputBoundaryFactory factory,std::string text={}) {
        return protect<TextInput>([&]() -> TextInputResult<TextInput> {
            TextInput result(std::move(factory));
            auto boundary = result.index(text); if (!boundary) return std::unexpected(boundary.error());
            result.text_ = std::move(text); result.boundary_ = std::move(*boundary);
            result.anchor_ = result.caret_ = result.length();
            return result;
        });
    }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    // During preedit this remains the value from before the composition.
    [[nodiscard]] const std::string& committed_text() const noexcept { return original_ ? original_->text : text_; }
    [[nodiscard]] std::size_t length() const noexcept { return boundary_->length(); }
    [[nodiscard]] std::size_t anchor() const noexcept { return anchor_; }
    [[nodiscard]] std::size_t caret() const noexcept { return caret_; }
    [[nodiscard]] TextInputRange selection() const noexcept {
        return {std::min(anchor_,caret_),std::max(anchor_,caret_)-std::min(anchor_,caret_)};
    }
    [[nodiscard]] std::optional<TextInputRange> marked() const noexcept { return marked_; }
    [[nodiscard]] TextInputResult<std::string> read(TextInputRange range) const {
        return protect<std::string>([&]() -> TextInputResult<std::string> {
            auto value = bytes(range); if (!value) return std::unexpected(value.error());
            return text_.substr(value->first,value->second-value->first);
        });
    }
    [[nodiscard]] TextInputResult<bool> set_text(std::string value) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            if (value == text_ && !marked_) return false;
            auto boundary = index(value); if (!boundary) return std::unexpected(boundary.error());
            const bool changed = value != text_ || bool(marked_);
            text_ = std::move(value); boundary_ = std::move(*boundary); anchor_ = caret_ = length();
            marked_.reset(); original_.reset(); return changed;
        });
    }
    [[nodiscard]] TextInputResult<bool> set_selection(std::size_t anchor,std::size_t caret) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            if (!bytes({std::min(anchor,caret),std::max(anchor,caret)-std::min(anchor,caret)}))
                return failure<bool>(TextInputErrorCode::invalid_range,"Selection must use complete Unicode scalars");
            const bool changed = anchor != anchor_ || caret != caret_; anchor_ = anchor; caret_ = caret; return changed;
        });
    }
    // A collapsed selection moves one extended grapheme or word. A non-extended
    // move first collapses an existing selection toward the requested direction.
    [[nodiscard]] TextInputResult<bool> move(int direction,bool extend,bool word=false) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            if (direction != -1 && direction != 1)
                return failure<bool>(TextInputErrorCode::invalid_range,"Text movement direction must be -1 or 1");
            const auto selected = selection();
            const auto next = !extend && selected.length ? (direction < 0 ? selected.location : selected.location + selected.length) :
                direction < 0 ? (word ? boundary_->previous_word(caret_) : boundary_->previous_grapheme(caret_)) :
                                (word ? boundary_->next_word(caret_) : boundary_->next_grapheme(caret_));
            if (next > length() || !boundary_->byte_offset(next) ||
                (selected.length == 0 && ((direction < 0 && next > caret_) || (direction > 0 && next < caret_))))
                return failure<bool>(TextInputErrorCode::invalid_boundary,"Text boundary service returned an invalid movement");
            return set_selection(extend ? anchor_ : next,next);
        });
    }
    [[nodiscard]] TextInputResult<bool> insert(std::string_view inserted,std::optional<TextInputRange> range=std::nullopt) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            const auto target = range.value_or(marked_.value_or(selection()));
            auto value = replace(target,inserted); if (!value) return std::unexpected(value.error());
            const bool changed = value->text != (original_ ? original_->text : text_);
            const auto position = target.location + value->inserted_length;
            text_ = std::move(value->text); boundary_ = std::move(value->boundary);
            anchor_ = caret_ = position; marked_.reset(); original_.reset(); return changed;
        });
    }
    [[nodiscard]] TextInputResult<bool> erase(int direction,bool word=false) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            if (marked_) return failure<bool>(TextInputErrorCode::composing,"Commit or cancel composition before deleting");
            if (direction != -1 && direction != 1)
                return failure<bool>(TextInputErrorCode::invalid_range,"Delete direction must be -1 or 1");
            auto target = selection();
            if (!target.length) {
                const auto edge = direction < 0 ? (word ? boundary_->previous_word(caret_) : boundary_->previous_grapheme(caret_)) :
                                                  (word ? boundary_->next_word(caret_) : boundary_->next_grapheme(caret_));
                if (edge > length() || !boundary_->byte_offset(edge) || (direction < 0 && edge > caret_) || (direction > 0 && edge < caret_))
                    return failure<bool>(TextInputErrorCode::invalid_boundary,"Text boundary service returned an invalid deletion");
                target = {std::min(caret_,edge),std::max(caret_,edge)-std::min(caret_,edge)};
            }
            return insert({},target);
        });
    }
    // Marked text is included in the visible document and native UTF-16 ranges.
    // It becomes an edit only on commit; cancel restores the complete original.
    [[nodiscard]] TextInputResult<bool> set_composition(std::string_view inserted,
            std::optional<TextInputRange> replacement=std::nullopt,std::optional<TextInputRange> selection=std::nullopt) {
        return protect<bool>([&]() -> TextInputResult<bool> {
            const auto target = replacement.value_or(marked_.value_or(this->selection()));
            auto value = replace(target,inserted); if (!value) return std::unexpected(value.error());
            const auto chosen = selection.value_or(TextInputRange{value->inserted_length,0});
            if (chosen.location > value->inserted_length || chosen.length > value->inserted_length - chosen.location ||
                !value->boundary->byte_offset(target.location + chosen.location) ||
                !value->boundary->byte_offset(target.location + chosen.location + chosen.length))
                return failure<bool>(TextInputErrorCode::invalid_range,"Composition selection splits a Unicode scalar");
            auto original = original_ ? *original_ : Original{text_,boundary_,anchor_,caret_};
            const TextInputRange next_marked{target.location,value->inserted_length};
            const auto next_anchor = target.location + chosen.location;
            const auto next_caret = next_anchor + chosen.length;
            const bool changed = text_ != value->text || marked_ != std::optional<TextInputRange>{next_marked} ||
                                 anchor_ != next_anchor || caret_ != next_caret;
            text_ = std::move(value->text); boundary_ = std::move(value->boundary);
            anchor_ = next_anchor; caret_ = next_caret; marked_ = next_marked; original_ = std::move(original);
            return changed;
        });
    }
    [[nodiscard]] TextInputResult<bool> commit_composition() {
        if (!original_) return false;
        const bool changed = text_ != original_->text;
        marked_.reset(); original_.reset(); return changed;
    }
    [[nodiscard]] TextInputResult<bool> cancel_composition() {
        if (!original_) return false;
        const bool changed = text_ != original_->text || anchor_ != original_->anchor || caret_ != original_->caret;
        text_ = std::move(original_->text); boundary_ = std::move(original_->boundary);
        anchor_ = original_->anchor; caret_ = original_->caret; marked_.reset(); original_.reset(); return changed;
    }
};
} // namespace lvui
