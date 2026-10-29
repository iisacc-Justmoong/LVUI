#include <LVUI/CodeDocument.h>
#include <iiSDKCore/Core/Text/Text.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::CodeDocumentResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
}
int main() {
    using namespace lvui;
    auto document = take(CodeMemoryDocument::create("A😀\n한\n", {.maximum_bytes=1024, .maximum_edit_bytes=128, .chunk_bytes=64}));
    auto original = take(document->snapshot());
    require(original->revision() == 0 && original->length() == 6 && original->line_count() == 3 &&
            take(original->line_start(0)) == 0 && take(original->line_start(1)) == 4 && take(original->line_start(2)) == 6 &&
            take(original->read({0, 6}, 64)) == "A😀\n한\n", "Code document UTF-16 positions or line index are incorrect");
    require(!original->read({2, 1}, 64) && !original->read({0, 6}, 2) && !original->line_start(3),
            "Code document accepted a split scalar, oversized read or nonexistent line");
    auto changed = take(document->replace({.revision=0, .range={1, 2}, .text="💡"}));
    require(changed.revision == 1 && changed.inserted_length == 2 && take(document->snapshot())->revision() == 1 &&
            take(original->read({0, 6}, 64)) == "A😀\n한\n" && take(take(document->snapshot())->read({0, 6}, 64)) == "A💡\n한\n",
            "A Unicode code edit lost immutable snapshot isolation");
    require(!document->replace({.revision=0, .range={0, 1}, .text="x"}) &&
            !document->replace({.revision=1, .range={2, 1}, .text="x"}) &&
            !document->replace({.revision=1, .range={0, 0}, .text=std::string("bad\xff", 4)}),
            "A stale edit, split Unicode scalar or invalid UTF-8 was accepted");
    require(take(document->replace({.revision=1, .range={1, 2}, .text="💡"})).revision == 1,
            "An unchanged code edit published a revision");
    require(!document->replace({.revision=1, .range={0, 0}, .text=std::string(129, 'x')}) &&
            !CodeMemoryDocument::create(std::string(1025, 'x'), {.maximum_bytes=1024, .maximum_edit_bytes=128, .chunk_bytes=64}),
            "Code document byte limits were not enforced");

    auto empty = take(CodeMemoryDocument::create("", {.maximum_bytes=1024*1024, .maximum_edit_bytes=4096, .chunk_bytes=64}));
    auto empty_view = take(empty->snapshot()); require(empty_view->length() == 0 && empty_view->line_count() == 1 && take(empty_view->line_start(0)) == 0,
            "An empty code document has no first line");
    std::string reference(500, 'a'); reference[80] = '\n'; reference[270] = '\n';
    auto state = take(empty->replace({.revision=0, .range={0, 0}, .text=reference})); std::mt19937 random(12345);
    std::vector<std::pair<std::shared_ptr<const CodeDocumentView>, std::string>> retained;
    for (int i = 0; i < 500; ++i) {
        if (i % 100 == 0) retained.emplace_back(take(empty->snapshot()), reference);
        const auto start = std::size_t(random() % (reference.size() + 1));
        const auto count = std::min<std::size_t>(random() % 8, reference.size() - start);
        std::string inserted(random() % 8, char('a' + random() % 26)); if (i % 17 == 0) inserted += '\n';
        state = take(empty->replace({.revision=state.revision, .range={start, count}, .text=inserted}));
        reference.replace(start, count, inserted); auto view = take(empty->snapshot());
        require(view->length() == reference.size() && take(view->read({0, reference.size()}, 1024*1024)) == reference,
                "Chunked code editing diverged from the expected text");
        const auto lines = std::size_t(std::count(reference.begin(), reference.end(), '\n')) + 1;
        require(view->line_count() == lines, "Chunked code editing lost line boundaries");
        std::size_t line = 1;
        for (std::size_t at = 0; at < reference.size(); ++at) if (reference[at] == '\n')
            require(take(view->line_start(line++)) == at + 1, "Chunked code line position is incorrect");
    }
    for (const auto& [view, text] : retained) require(take(view->read({0, text.size()}, 1024*1024)) == text,
        "Later code edits changed a retained immutable snapshot");
    std::string mixed; for (int i = 0; i < 150; ++i) mixed += i % 3 == 0 ? "😀" : i % 3 == 1 ? "한" : "\n";
    auto unicode = take(CodeMemoryDocument::create(mixed, {.maximum_bytes=1024*1024, .maximum_edit_bytes=4096, .chunk_bytes=64}));
    auto reference16 = ii::core::to_utf16(mixed).value(); std::uint64_t unicode_revision = 0;
    const std::vector<std::string> inserts{"", "😀", "한", "\n", "ab"};
    for (int iteration = 0; iteration < 200; ++iteration) {
        std::vector<std::size_t> boundaries;
        for (std::size_t i = 0; i <= reference16.size(); ++i)
            if (i == reference16.size() || reference16[i] < 0xdc00 || reference16[i] > 0xdfff) boundaries.push_back(i);
        const auto first = std::size_t(random() % boundaries.size());
        const auto last = std::min(boundaries.size() - 1, first + std::size_t(random() % 5));
        const auto& text = inserts[random() % inserts.size()];
        const auto result = take(unicode->replace({.revision=unicode_revision,
            .range={boundaries[first], boundaries[last] - boundaries[first]}, .text=text}));
        unicode_revision = result.revision; reference16.replace(boundaries[first], boundaries[last] - boundaries[first], ii::core::to_utf16(text).value());
        auto view = take(unicode->snapshot());
        require(view->length() == reference16.size() && take(view->read({0, reference16.size()}, 1024*1024)) == ii::core::to_utf8(reference16).value(),
                "Unicode edits across chunks corrupted text or UTF-16 positions");
        std::size_t line = 1;
        for (std::size_t i = 0; i < reference16.size(); ++i) if (reference16[i] == u'\n')
            require(take(view->line_start(line++)) == i + 1, "Unicode code line position is incorrect");
        require(view->line_count() == line, "Unicode code line count is incorrect");
    }
    const auto revision = take(empty->snapshot())->revision(); std::atomic<int> winners{0};
    std::thread a([&] { if (empty->replace({.revision=revision, .range={0, 0}, .text="A"})) ++winners; });
    std::thread b([&] { if (empty->replace({.revision=revision, .range={0, 0}, .text="B"})) ++winners; });
    a.join(); b.join(); require(winners == 1 && take(empty->snapshot())->revision() == revision + 1,
        "Concurrent code edits both committed the same revision");
}
