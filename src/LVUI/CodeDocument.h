#pragma once

#include <iiSDKCore/Core/Text/Utf8.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lvui {

struct CodeDocumentRange final { std::size_t location = 0, length = 0; };
enum class CodeDocumentErrorCode { invalid, out_of_range, too_large, stale, exhausted, wrong_thread, busy };
struct CodeDocumentError final { CodeDocumentErrorCode code; std::string message; };
template<class T> using CodeDocumentResult = std::expected<T, CodeDocumentError>;
struct CodeDocumentLimits final {
    std::size_t maximum_bytes = 64 * 1024 * 1024, maximum_edit_bytes = 4 * 1024 * 1024, chunk_bytes = 4096;
};
struct CodeDocumentEdit final { std::uint64_t revision = 0; CodeDocumentRange range; std::string text; };
struct CodeDocumentChange final {
    std::uint64_t before_revision = 0, revision = 0;
    CodeDocumentRange removed;
    std::size_t inserted_length = 0;
};

// Views are immutable and can be retained by renderers or background readers.
// Locations use UTF-16 units, while text supplied and returned is UTF-8.
class CodeDocumentView {
public:
    virtual ~CodeDocumentView() = default;
    [[nodiscard]] virtual std::uint64_t revision() const noexcept = 0;
    [[nodiscard]] virtual std::size_t length() const noexcept = 0;
    [[nodiscard]] virtual std::size_t byte_size() const noexcept = 0;
    [[nodiscard]] virtual std::size_t line_count() const noexcept = 0;
    [[nodiscard]] virtual CodeDocumentResult<std::size_t> line_start(std::size_t line) const = 0;
    [[nodiscard]] virtual CodeDocumentResult<std::string> read(CodeDocumentRange range, std::size_t maximum_bytes) const = 0;
    // Sources can override this for indexed lookup; the default reads bounded
    // slices so a large selected range never requires a large temporary string.
    [[nodiscard]] virtual CodeDocumentResult<std::size_t> range_byte_size(CodeDocumentRange range) const {
        if (range.location > length() || range.length > length() - range.location)
            return std::unexpected(CodeDocumentError{CodeDocumentErrorCode::out_of_range, "Code range exceeds the document"});
        auto start = read({range.location, 0}, 0); if (!start) return std::unexpected(start.error());
        auto end = read({range.location + range.length, 0}, 0); if (!end) return std::unexpected(end.error());
        std::size_t position = range.location, result = 0;
        const auto finish = range.location + range.length;
        while (position < finish) {
            auto count = std::min<std::size_t>(65536, finish - position);
            auto part = read({position, count}, 4 * 65536);
            if (!part && part.error().code == CodeDocumentErrorCode::out_of_range && count > 1) {
                --count; part = read({position, count}, 4 * 65536);
            }
            if (!part) return std::unexpected(part.error());
            if (part->size() > std::numeric_limits<std::size_t>::max() - result)
                return std::unexpected(CodeDocumentError{CodeDocumentErrorCode::too_large, "Code range byte count overflow"});
            result += part->size(); position += count;
        }
        return result;
    }
};
class CodeDocumentSource {
public:
    virtual ~CodeDocumentSource() = default;
    [[nodiscard]] virtual CodeDocumentResult<std::shared_ptr<const CodeDocumentView>> snapshot() const = 0;
};
class CodeDocumentEditor {
public:
    virtual ~CodeDocumentEditor() = default;
    [[nodiscard]] virtual CodeDocumentResult<CodeDocumentChange> replace(const CodeDocumentEdit&) = 0;
};

namespace code_document_detail {
inline constexpr std::size_t maximum_chunks = 65536, maximum_read_bytes = 4 * 1024 * 1024;
[[nodiscard]] inline std::unexpected<CodeDocumentError> error(CodeDocumentErrorCode code, std::string message) {
    return std::unexpected(CodeDocumentError{code, std::move(message)});
}
[[nodiscard]] inline bool valid_text(std::string_view value) noexcept {
    return value.find('\0') == value.npos && ii::core::valid_utf8(value);
}
[[nodiscard]] inline std::size_t scalar_bytes(unsigned char lead) noexcept {
    return lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
}
struct Chunk final {
    std::string text;
    std::size_t units = 0, newlines = 0;
};
[[nodiscard]] inline CodeDocumentResult<std::vector<std::shared_ptr<const Chunk>>> chunks(std::string_view text, std::size_t limit) {
    std::vector<std::shared_ptr<const Chunk>> result;
    for (std::size_t first = 0; first < text.size();) {
        auto last = first; std::size_t units = 0, lines = 0;
        while (last < text.size()) {
            const auto size = scalar_bytes(static_cast<unsigned char>(text[last]));
            if (last > first && size > limit - (last - first)) break;
            if (text[last] == '\n') ++lines;
            units += size == 4 ? 2 : 1; last += size;
        }
        if (result.size() == maximum_chunks) return error(CodeDocumentErrorCode::too_large, "Code document exceeds its chunk limit");
        result.push_back(std::make_shared<const Chunk>(Chunk{std::string(text.substr(first, last - first)), units, lines})); first = last;
    }
    return result;
}

class View final : public CodeDocumentView {
public:
    struct Position final { std::size_t chunk = 0, byte = 0; };
    View(std::uint64_t revision, std::vector<std::shared_ptr<const Chunk>> values) : revision_(revision), chunks_(std::move(values)) {
        units_.reserve(chunks_.size() + 1); bytes_.reserve(chunks_.size() + 1); lines_.reserve(chunks_.size() + 1);
        units_.push_back(0); bytes_.push_back(0); lines_.push_back(0);
        for (const auto& chunk : chunks_) { units_.push_back(units_.back() + chunk->units); bytes_.push_back(bytes_.back() + chunk->text.size());
            lines_.push_back(lines_.back() + chunk->newlines); }
    }
    [[nodiscard]] std::uint64_t revision() const noexcept override { return revision_; }
    [[nodiscard]] std::size_t length() const noexcept override { return units_.back(); }
    [[nodiscard]] std::size_t byte_size() const noexcept override { return bytes_.back(); }
    [[nodiscard]] std::size_t line_count() const noexcept override { return lines_.back() + 1; }
    [[nodiscard]] CodeDocumentResult<Position> locate(std::size_t position) const {
        if (position > length()) return error(CodeDocumentErrorCode::out_of_range, "Code position exceeds the document");
        if (position == length()) return Position{chunks_.size(), 0};
        const auto found = std::upper_bound(units_.begin(), units_.end(), position);
        const auto index = std::size_t(found - units_.begin() - 1); auto remaining = position - units_[index];
        const auto& text = chunks_[index]->text; std::size_t at = 0;
        while (remaining) {
            const auto size = scalar_bytes(static_cast<unsigned char>(text[at])); const std::size_t width = size == 4 ? 2 : 1;
            if (remaining < width) return error(CodeDocumentErrorCode::out_of_range, "Code position splits a Unicode scalar");
            at += size; remaining -= width;
        }
        return Position{index, at};
    }
    [[nodiscard]] std::size_t absolute_byte(Position value) const noexcept { return bytes_[value.chunk] + value.byte; }
    [[nodiscard]] CodeDocumentResult<std::size_t> range_byte_size(CodeDocumentRange range) const override {
        if (range.location > length() || range.length > length() - range.location)
            return error(CodeDocumentErrorCode::out_of_range, "Code range exceeds the document");
        auto first = locate(range.location), last = locate(range.location + range.length);
        if (!first) return std::unexpected(first.error()); if (!last) return std::unexpected(last.error());
        return absolute_byte(*last) - absolute_byte(*first);
    }
    [[nodiscard]] CodeDocumentResult<std::string> read(CodeDocumentRange range, std::size_t maximum_bytes) const override {
        if (maximum_bytes > maximum_read_bytes) return error(CodeDocumentErrorCode::too_large, "Code read limit exceeds 4 MiB");
        if (range.location > length() || range.length > length() - range.location)
            return error(CodeDocumentErrorCode::out_of_range, "Code range exceeds the document");
        auto first = locate(range.location), last = locate(range.location + range.length);
        if (!first) return std::unexpected(first.error()); if (!last) return std::unexpected(last.error());
        const auto count = absolute_byte(*last) - absolute_byte(*first);
        if (count > maximum_bytes) return error(CodeDocumentErrorCode::too_large, "Code range exceeds its read budget");
        std::string result; result.reserve(count);
        for (auto i = first->chunk; i < chunks_.size() && i <= last->chunk; ++i) {
            const auto begin = i == first->chunk ? first->byte : 0;
            const auto end = i == last->chunk ? last->byte : chunks_[i]->text.size();
            result.append(chunks_[i]->text, begin, end - begin);
        }
        return result;
    }
    [[nodiscard]] CodeDocumentResult<std::size_t> line_start(std::size_t line) const override {
        if (line >= line_count()) return error(CodeDocumentErrorCode::out_of_range, "Code line does not exist");
        if (!line) return std::size_t(0);
        const auto target = line - 1;
        const auto found = std::upper_bound(lines_.begin(), lines_.end(), target);
        const auto index = std::size_t(found - lines_.begin() - 1); const auto& text = chunks_[index]->text;
        auto remaining = target - lines_[index]; std::size_t at = 0, units = units_[index];
        while (at < text.size()) {
            const auto size = scalar_bytes(static_cast<unsigned char>(text[at]));
            if (text[at] == '\n') { if (!remaining) return units + 1; --remaining; }
            units += size == 4 ? 2 : 1; at += size;
        }
        return error(CodeDocumentErrorCode::invalid, "Code line index is inconsistent");
    }
    [[nodiscard]] bool equals(Position first, Position last, std::string_view value) const noexcept {
        if (absolute_byte(last) - absolute_byte(first) != value.size()) return false;
        std::size_t offset = 0;
        for (auto i = first.chunk; i < chunks_.size() && i <= last.chunk; ++i) {
            const auto begin = i == first.chunk ? first.byte : 0, end = i == last.chunk ? last.byte : chunks_[i]->text.size();
            const auto part = std::string_view(chunks_[i]->text).substr(begin, end - begin);
            if (value.substr(offset, part.size()) != part) return false; offset += part.size();
        }
        return offset == value.size();
    }
    [[nodiscard]] const std::vector<std::shared_ptr<const Chunk>>& parts() const noexcept { return chunks_; }
private:
    std::uint64_t revision_;
    std::vector<std::shared_ptr<const Chunk>> chunks_;
    std::vector<std::size_t> units_, bytes_, lines_;
};
} // namespace code_document_detail

// A bounded in-memory implementation. Edits share unchanged chunks with every
// retained snapshot and rebuild only the chunks around the changed range.
class CodeMemoryDocument final : public CodeDocumentSource, public CodeDocumentEditor {
public:
    [[nodiscard]] static CodeDocumentResult<std::shared_ptr<CodeMemoryDocument>> create(std::string text = {}, CodeDocumentLimits limits = {}) {
        using namespace code_document_detail;
        if (!limits.maximum_bytes || limits.maximum_bytes > 64 * 1024 * 1024 || !limits.maximum_edit_bytes ||
            limits.maximum_edit_bytes > 64 * 1024 * 1024 || limits.chunk_bytes < 64 || limits.chunk_bytes > 16384)
            return error(CodeDocumentErrorCode::invalid, "Invalid code document limits");
        if (text.size() > limits.maximum_bytes) return error(CodeDocumentErrorCode::too_large, "Code document exceeds its byte limit");
        if (!valid_text(text)) return error(CodeDocumentErrorCode::invalid, "Code document requires NUL-free UTF-8");
        auto parts = chunks(text, limits.chunk_bytes);
        if (!parts) return std::unexpected(parts.error());
        return std::shared_ptr<CodeMemoryDocument>(new CodeMemoryDocument(limits, std::make_shared<const View>(0, std::move(*parts))));
    }
    [[nodiscard]] CodeDocumentResult<std::shared_ptr<const CodeDocumentView>> snapshot() const override {
        std::scoped_lock lock(mutex_); return std::shared_ptr<const CodeDocumentView>(snapshot_);
    }
    [[nodiscard]] CodeDocumentResult<CodeDocumentChange> replace(const CodeDocumentEdit& edit) override {
        using namespace code_document_detail;
        if (edit.text.size() > limits_.maximum_edit_bytes) return error(CodeDocumentErrorCode::too_large, "Code edit exceeds its byte limit");
        if (!valid_text(edit.text)) return error(CodeDocumentErrorCode::invalid, "Code edit requires NUL-free UTF-8");
        std::scoped_lock lock(mutex_); const auto current = snapshot_;
        if (current->revision() != edit.revision) return error(CodeDocumentErrorCode::stale, "Code document changed before the edit");
        if (edit.range.location > current->length() || edit.range.length > current->length() - edit.range.location)
            return error(CodeDocumentErrorCode::out_of_range, "Code edit range exceeds the document");
        auto first = current->locate(edit.range.location), last = current->locate(edit.range.location + edit.range.length);
        if (!first) return std::unexpected(first.error()); if (!last) return std::unexpected(last.error());
        std::size_t inserted_units = 0;
        for (std::size_t i = 0; i < edit.text.size();) { const auto size = scalar_bytes(static_cast<unsigned char>(edit.text[i])); inserted_units += size == 4 ? 2 : 1; i += size; }
        CodeDocumentChange change{edit.revision, edit.revision, edit.range, inserted_units};
        if (current->equals(*first, *last, edit.text)) return change;
        const auto removed = current->absolute_byte(*last) - current->absolute_byte(*first);
        if (edit.text.size() > limits_.maximum_bytes - (current->byte_size() - removed))
            return error(CodeDocumentErrorCode::too_large, "Code document exceeds its byte limit");
        if (edit.revision == std::numeric_limits<std::uint64_t>::max()) return error(CodeDocumentErrorCode::exhausted, "Code document revision exhausted");
        const auto& old = current->parts(); const auto count = old.size();
        const auto low = first->chunk ? first->chunk - 1 : 0;
        const auto high = last->chunk < count ? std::min(count, last->chunk + 2) : count;
        std::string local;
        for (auto i = low; i < first->chunk; ++i) local += old[i]->text;
        if (first->chunk < count) local.append(old[first->chunk]->text, 0, first->byte);
        local += edit.text;
        if (last->chunk < count) {
            local.append(old[last->chunk]->text, last->byte, std::string::npos);
            for (auto i = last->chunk + 1; i < high; ++i) local += old[i]->text;
        }
        auto middle = chunks(local, limits_.chunk_bytes);
        if (!middle) return std::unexpected(middle.error());
        if (low + middle->size() + (count - high) > maximum_chunks)
            return error(CodeDocumentErrorCode::too_large, "Code document exceeds its chunk limit");
        std::vector<std::shared_ptr<const Chunk>> next; next.reserve(low + middle->size() + count - high);
        next.insert(next.end(), old.begin(), old.begin() + std::ptrdiff_t(low));
        next.insert(next.end(), std::make_move_iterator(middle->begin()), std::make_move_iterator(middle->end()));
        next.insert(next.end(), old.begin() + std::ptrdiff_t(high), old.end());
        change.revision = edit.revision + 1; snapshot_ = std::make_shared<const View>(change.revision, std::move(next)); return change;
    }
private:
    CodeMemoryDocument(CodeDocumentLimits limits, std::shared_ptr<const code_document_detail::View> view)
        : limits_(limits), snapshot_(std::move(view)) {}
    CodeDocumentLimits limits_;
    mutable std::mutex mutex_;
    std::shared_ptr<const code_document_detail::View> snapshot_;
};

} // namespace lvui
