#pragma once

#include <LVUI/CodeDocument.h>

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

// The application binds these document-facing values to its GUI planner.
struct CodeViewportPolicy final {
    using DocumentView=CodeDocumentView;
    using ErrorCode=CodeDocumentErrorCode;
    using Request=CodeViewportRequest;
    using Frame=CodeViewportFrame;
    template<class T>using Result=CodeDocumentResult<T>;
    static constexpr std::size_t maximum_read_bytes=code_document_detail::maximum_read_bytes;
    [[nodiscard]] static auto error(ErrorCode code,std::string message){return code_document_detail::error(code,std::move(message));}
    [[nodiscard]] static auto line_bounds(const DocumentView& view,std::size_t line){return code_line_bounds(view,line);}
};
template<template<class>class Engine>using CodeViewportBinding=Engine<CodeViewportPolicy>;

} // namespace lvui
