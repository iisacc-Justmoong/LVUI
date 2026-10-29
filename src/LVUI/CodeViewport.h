#pragma once

#include "CodeDocument.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace lvui {

struct CodeLineBounds final { std::size_t start = 0, end = 0; };

// Both helpers operate on immutable views; no complete document copy is needed.
[[nodiscard]] inline CodeDocumentResult<std::size_t> code_line_at(const CodeDocumentView& view, std::size_t position) {
    using code_document_detail::error;
    if (!view.line_count()) return error(CodeDocumentErrorCode::invalid, "Code source has no first line");
    if (position > view.length()) return error(CodeDocumentErrorCode::out_of_range, "Code position exceeds the document");
    std::size_t first = 0, last = view.line_count();
    while (first < last) {
        const auto middle = first + (last - first) / 2;
        auto start = view.line_start(middle); if (!start) return std::unexpected(start.error());
        if (*start <= position) first = middle + 1; else last = middle;
    }
    if (!first) return error(CodeDocumentErrorCode::invalid, "Code source has no first line");
    return first - 1;
}

// The logical line excludes the LF and, for CRLF, its preceding CR.
[[nodiscard]] inline CodeDocumentResult<CodeLineBounds> code_line_bounds(const CodeDocumentView& view, std::size_t line) {
    using code_document_detail::error;
    auto first = view.line_start(line); if (!first) return std::unexpected(first.error());
    auto last = view.length();
    if (line + 1 < view.line_count()) {
        auto next = view.line_start(line + 1); if (!next) return std::unexpected(next.error());
        if (*next <= *first || *next > view.length()) return error(CodeDocumentErrorCode::invalid, "Invalid code line bounds");
        last = *next - 1;
        if (last > *first) {
            auto previous = view.read({last - 1, 1}, 4);
            if (previous && *previous == "\r") --last;
            else if (!previous && previous.error().code != CodeDocumentErrorCode::out_of_range) return std::unexpected(previous.error());
        }
    }
    if (*first > last) return error(CodeDocumentErrorCode::invalid, "Invalid code line bounds");
    return CodeLineBounds{*first, last};
}

struct CodeViewportRequest final {
    std::size_t first_line = 0, line_count = 40, first_column = 0, line_units = 4096;
    std::size_t maximum_bytes = 1024 * 1024;
};
struct CodeViewportLine final {
    std::size_t line = 0, start = 0, end = 0, visible_start = 0, visible_end = 0;
    std::string text;
    bool clipped_before = false, clipped_after = false;
};
struct CodeViewportFrame final {
    std::uint64_t revision = 0;
    std::size_t total_lines = 0;
    std::vector<CodeViewportLine> lines;
};

// A renderer chooses a logical line/UTF-16 column window. Each request is
// bounded by both the line count and total UTF-8 bytes; Unicode scalars are
// never split at either edge. The caller may keep the originating snapshot.
[[nodiscard]] inline CodeDocumentResult<CodeViewportFrame> code_viewport(const CodeDocumentView& view, CodeViewportRequest request = {}) {
    using code_document_detail::error;
    if (!view.line_count()) return error(CodeDocumentErrorCode::invalid, "Code source has no first line");
    if (request.first_line > view.line_count() || request.line_count > 512 || request.line_units < 2 ||
        request.line_units > 65536 || request.maximum_bytes > code_document_detail::maximum_read_bytes)
        return error(CodeDocumentErrorCode::invalid, "Invalid code viewport request");
    CodeViewportFrame result{.revision=view.revision(), .total_lines=view.line_count()};
    const auto count = std::min(request.line_count, result.total_lines - request.first_line);
    result.lines.reserve(count); auto remaining = request.maximum_bytes;
    for (std::size_t offset = 0; offset < count; ++offset) {
        const auto line = request.first_line + offset;
        auto bounds = code_line_bounds(view, line); if (!bounds) return std::unexpected(bounds.error());
        auto first = bounds->start + std::min(request.first_column, bounds->end - bounds->start);
        auto boundary = view.read({first, 0}, 0);
        if (!boundary && boundary.error().code == CodeDocumentErrorCode::out_of_range && first > bounds->start) {
            --first; boundary = view.read({first, 0}, 0);
        }
        if (!boundary) return std::unexpected(boundary.error());
        auto last = first + std::min(request.line_units, bounds->end - first);
        boundary = view.read({last, 0}, 0);
        if (!boundary && boundary.error().code == CodeDocumentErrorCode::out_of_range && last > first) {
            --last; boundary = view.read({last, 0}, 0);
        }
        if (!boundary) return std::unexpected(boundary.error());
        auto text = view.read({first, last - first}, remaining); if (!text) return std::unexpected(text.error());
        if (text->size() > remaining) return error(CodeDocumentErrorCode::invalid, "Code source exceeded the viewport read budget");
        remaining -= text->size();
        result.lines.push_back({line, bounds->start, bounds->end, first, last, std::move(*text),
            first > bounds->start, last < bounds->end});
    }
    return result;
}

} // namespace lvui
