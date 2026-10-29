#pragma once

#include <LVUI/CodeViewport.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace lvui {
namespace code_composition_detail {

// Presents the uncommitted replacement as an immutable view. Untouched text
// remains in the original document, even when the selected range is large.
class View final : public CodeDocumentView {
public:
    View(std::shared_ptr<const CodeDocumentView> base, CodeDocumentRange range, std::string text,
         std::size_t units, std::size_t bytes, std::size_t first_line, std::size_t last_line,
         std::vector<std::size_t> line_offsets)
        : base_(std::move(base)), range_(range), text_(std::move(text)), units_(units), bytes_(bytes),
          first_line_(first_line), last_line_(last_line), line_offsets_(std::move(line_offsets)) {}
    [[nodiscard]] std::uint64_t revision() const noexcept override { return base_->revision(); }
    [[nodiscard]] std::size_t length() const noexcept override { return base_->length() - range_.length + units_; }
    [[nodiscard]] std::size_t byte_size() const noexcept override { return bytes_; }
    [[nodiscard]] std::size_t line_count() const noexcept override { return base_->line_count() - (last_line_ - first_line_) + line_offsets_.size(); }
    [[nodiscard]] const std::string& inserted() const noexcept { return text_; }
    [[nodiscard]] std::size_t inserted_units() const noexcept { return units_; }
    [[nodiscard]] CodeDocumentRange original_range() const noexcept { return range_; }
    [[nodiscard]] CodeDocumentResult<std::size_t> line_start(std::size_t line) const override {
        using code_document_detail::error;
        if (line >= line_count()) return error(CodeDocumentErrorCode::out_of_range, "Code line does not exist");
        if (line <= first_line_) return base_->line_start(line);
        if (line <= first_line_ + line_offsets_.size()) return range_.location + line_offsets_[line - first_line_ - 1];
        const auto base_line = last_line_ + (line - first_line_ - line_offsets_.size());
        auto start = base_->line_start(base_line); if (!start) return std::unexpected(start.error());
        const auto removed_end = range_.location + range_.length;
        if (*start < removed_end) return error(CodeDocumentErrorCode::invalid, "Invalid composition line index");
        return range_.location + units_ + (*start - removed_end);
    }
    [[nodiscard]] CodeDocumentResult<std::size_t> inserted_byte(std::size_t unit) const {
        using code_document_detail::error;
        if (unit > units_) return error(CodeDocumentErrorCode::out_of_range, "Composition position exceeds its text");
        std::size_t at = 0, current = 0;
        while (current < unit) {
            const auto size = code_document_detail::scalar_bytes(static_cast<unsigned char>(text_[at]));
            const std::size_t width = size == 4 ? 2 : 1;
            if (width > unit - current) return error(CodeDocumentErrorCode::out_of_range, "Composition position splits a Unicode scalar");
            at += size; current += width;
        }
        return at;
    }
    [[nodiscard]] CodeDocumentResult<std::string> read(CodeDocumentRange range, std::size_t maximum_bytes) const override {
        using code_document_detail::error;
        if (maximum_bytes > code_document_detail::maximum_read_bytes)
            return error(CodeDocumentErrorCode::too_large, "Code read limit exceeds 4 MiB");
        if (range.location > length() || range.length > length() - range.location)
            return error(CodeDocumentErrorCode::out_of_range, "Code range exceeds the document");
        const auto inserted_end = range_.location + units_, removed_end = range_.location + range_.length;
        const auto valid = [&](std::size_t at) -> CodeDocumentResult<void> {
            if (at < range_.location) { auto r = base_->read({at, 0}, 0); if (!r) return std::unexpected(r.error()); }
            else if (at <= inserted_end) { auto r = inserted_byte(at - range_.location); if (!r) return std::unexpected(r.error()); }
            else { auto r = base_->read({removed_end + at - inserted_end, 0}, 0); if (!r) return std::unexpected(r.error()); }
            return {};
        };
        auto begin_valid = valid(range.location); if (!begin_valid) return std::unexpected(begin_valid.error());
        auto end_valid = valid(range.location + range.length); if (!end_valid) return std::unexpected(end_valid.error());
        std::string result; const auto end = range.location + range.length; auto at = range.location;
        const auto append_base = [&](std::size_t start, std::size_t count) -> CodeDocumentResult<void> {
            auto part = base_->read({start, count}, maximum_bytes - result.size()); if (!part) return std::unexpected(part.error());
            if (part->size() > maximum_bytes - result.size()) return error(CodeDocumentErrorCode::invalid, "Code source exceeded the read budget");
            result += *part; return {};
        };
        if (at < range_.location) {
            const auto stop = std::min(end, range_.location);
            if (auto part = append_base(at, stop - at); !part) return std::unexpected(part.error()); at = stop;
        }
        if (at < end && at < inserted_end) {
            const auto stop = std::min(end, inserted_end);
            auto first = inserted_byte(at - range_.location), last = inserted_byte(stop - range_.location);
            if (!first) return std::unexpected(first.error()); if (!last) return std::unexpected(last.error());
            if (*last - *first > maximum_bytes - result.size()) return error(CodeDocumentErrorCode::too_large, "Code range exceeds its read budget");
            result.append(text_, *first, *last - *first); at = stop;
        }
        if (at < end) {
            if (auto part = append_base(removed_end + at - inserted_end, end - at); !part) return std::unexpected(part.error());
        }
        return result;
    }
private:
    std::shared_ptr<const CodeDocumentView> base_;
    CodeDocumentRange range_;
    std::string text_;
    std::size_t units_, bytes_, first_line_, last_line_;
    std::vector<std::size_t> line_offsets_;
};
} // namespace code_composition_detail

// Composition owns only a bounded candidate string and a projected view; it
// does not edit the base document. A caller commits it through its editor.
class CodeComposition final {
public:
    static constexpr std::size_t maximum_bytes = 65536;
    [[nodiscard]] static CodeDocumentResult<CodeComposition> create(std::shared_ptr<const CodeDocumentView> base,
            CodeDocumentRange range, std::string text, CodeDocumentRange selection) {
        using code_document_detail::error;
        if (!base) return error(CodeDocumentErrorCode::invalid, "Composition requires a document snapshot");
        if (range.location > base->length() || range.length > base->length() - range.location)
            return error(CodeDocumentErrorCode::out_of_range, "Composition range exceeds the document");
        if (text.size() > maximum_bytes) return error(CodeDocumentErrorCode::too_large, "Composition exceeds 64 KiB");
        if (!code_document_detail::valid_text(text)) return error(CodeDocumentErrorCode::invalid, "Composition requires NUL-free UTF-8");
        auto removed_bytes = base->range_byte_size(range); if (!removed_bytes) return std::unexpected(removed_bytes.error());
        if (*removed_bytes > base->byte_size()) return error(CodeDocumentErrorCode::invalid, "Invalid code source byte count");
        std::size_t units = 0; std::vector<std::size_t> line_offsets;
        for (std::size_t at = 0; at < text.size();) {
            const auto size = code_document_detail::scalar_bytes(static_cast<unsigned char>(text[at]));
            units += size == 4 ? 2 : 1; if (text[at] == '\n') line_offsets.push_back(units); at += size;
        }
        if (units > std::numeric_limits<std::size_t>::max() - (base->length() - range.length) ||
            text.size() > std::numeric_limits<std::size_t>::max() - (base->byte_size() - *removed_bytes))
            return error(CodeDocumentErrorCode::too_large, "Composition size overflow");
        if (selection.location > units || selection.length > units - selection.location)
            return error(CodeDocumentErrorCode::out_of_range, "Composition selection exceeds its text");
        auto first_line = code_line_at(*base, range.location), last_line = code_line_at(*base, range.location + range.length);
        if (!first_line) return std::unexpected(first_line.error()); if (!last_line) return std::unexpected(last_line.error());
        if (*last_line < *first_line || line_offsets.size() > std::numeric_limits<std::size_t>::max() - base->line_count())
            return error(CodeDocumentErrorCode::invalid, "Invalid code source line index");
        const auto bytes = base->byte_size() - *removed_bytes + text.size();
        auto view = std::make_shared<const code_composition_detail::View>(std::move(base), range, std::move(text), units, bytes,
            *first_line, *last_line, std::move(line_offsets));
        auto first = view->inserted_byte(selection.location), last = view->inserted_byte(selection.location + selection.length);
        if (!first) return std::unexpected(first.error()); if (!last) return std::unexpected(last.error());
        return CodeComposition(std::move(view), selection);
    }
    [[nodiscard]] std::shared_ptr<const CodeDocumentView> view() const { return view_; }
    [[nodiscard]] const std::string& text() const noexcept { return view_->inserted(); }
    [[nodiscard]] CodeDocumentRange base_range() const noexcept { return view_->original_range(); }
    [[nodiscard]] CodeDocumentRange marked() const noexcept { return {base_range().location, view_->inserted_units()}; }
    [[nodiscard]] CodeDocumentRange selection() const noexcept { return {base_range().location + selection_.location, selection_.length}; }
private:
    CodeComposition(std::shared_ptr<const code_composition_detail::View> view, CodeDocumentRange selection)
        : view_(std::move(view)), selection_(selection) {}
    std::shared_ptr<const code_composition_detail::View> view_;
    CodeDocumentRange selection_;
};

} // namespace lvui
