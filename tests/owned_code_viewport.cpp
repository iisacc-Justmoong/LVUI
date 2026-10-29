#include <LVUI/CodeViewport.h>

#include <limits>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::CodeDocumentResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
}
int main() {
    using namespace lvui;
    auto document = take(CodeMemoryDocument::create("a😀bc\r\n한글\n\nfinal\n"));
    auto snapshot = take(document->snapshot());
    require(take(code_line_at(*snapshot, 2)) == 0 && take(code_line_at(*snapshot, 7)) == 1 &&
            take(code_line_at(*snapshot, snapshot->length())) == 4 && !code_line_at(*snapshot, snapshot->length() + 1),
            "Code line lookup did not follow the UTF-16 document boundaries");
    auto frame = take(code_viewport(*snapshot, {.first_line=0, .line_count=3, .first_column=2, .line_units=2}));
    require(frame.revision == 0 && frame.total_lines == 5 && frame.lines.size() == 3 &&
            frame.lines[0].line == 0 && frame.lines[0].start == 0 && frame.lines[0].end == 5 &&
            frame.lines[0].visible_start == 1 && frame.lines[0].visible_end == 3 && frame.lines[0].text == "😀" &&
            frame.lines[0].clipped_before && frame.lines[0].clipped_after && frame.lines[1].text.empty() &&
            frame.lines[2].text.empty(), "Code viewport split a scalar, included CRLF, or returned the wrong slice");
    auto last = take(code_viewport(*snapshot, {.first_line=3, .line_count=8, .line_units=8}));
    require(last.lines.size() == 2 && last.lines[0].text == "final" && last.lines[1].text.empty() &&
            !last.lines[0].clipped_before && !last.lines[0].clipped_after, "Code viewport mishandled the final empty line");
    require(take(code_viewport(*snapshot, {.first_line=5, .line_count=1})).lines.empty() &&
            !code_viewport(*snapshot, {.first_line=6, .line_count=1}) &&
            !code_viewport(*snapshot, {.line_count=513}) && !code_viewport(*snapshot, {.line_units=1}) &&
            !code_viewport(*snapshot, {.line_units=65537}) && !code_viewport(*snapshot, {.maximum_bytes=4 * 1024 * 1024 + 1}) &&
            !code_viewport(*snapshot, {.line_count=1, .line_units=8, .maximum_bytes=3}),
            "Code viewport accepted an invalid or unbounded request");
    std::string text(200000, 'a'); text += "😀end\n"; for (int i = 0; i < 10000; ++i) text += "x\n";
    auto large = take(CodeMemoryDocument::create(std::move(text))); auto view = take(large->snapshot());
    auto segment = take(code_viewport(*view, {.line_count=1, .first_column=200001, .line_units=4}));
    require(segment.lines.size() == 1 && segment.lines[0].visible_start == 200000 && segment.lines[0].text == "😀en" &&
            segment.lines[0].clipped_after, "A very long code line was not sliced at scalar boundaries");
    auto deep = take(code_viewport(*view, {.first_line=9000, .line_count=10}));
    require(deep.lines.size() == 10 && deep.lines.front().line == 9000 && deep.lines.back().text == "x",
            "Code viewport materialized unrelated lines in a large document");
    auto out = take(code_viewport(*view, {.line_count=1, .first_column=std::numeric_limits<std::size_t>::max()}));
    require(out.lines[0].text.empty() && out.lines[0].visible_end == out.lines[0].end,
            "Code viewport column arithmetic overflowed");
}
