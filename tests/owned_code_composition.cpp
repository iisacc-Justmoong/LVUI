#include <LVUI/CodeComposition.h>
#include <LVUI/CodeEditorController.h>

#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(lvui::CodeDocumentResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
class GenericView final : public lvui::CodeDocumentView {
public:
    explicit GenericView(std::shared_ptr<const lvui::CodeDocumentView> value) : value_(std::move(value)) {}
    std::uint64_t revision() const noexcept override { return value_->revision(); }
    std::size_t length() const noexcept override { return value_->length(); }
    std::size_t byte_size() const noexcept override { return value_->byte_size(); }
    std::size_t line_count() const noexcept override { return value_->line_count(); }
    lvui::CodeDocumentResult<std::size_t> line_start(std::size_t line) const override { return value_->line_start(line); }
    lvui::CodeDocumentResult<std::string> read(lvui::CodeDocumentRange range, std::size_t maximum) const override {
        require(maximum <= 4 * 65536, "Generic code view received an unbounded read");
        return value_->read(range, maximum);
    }
private:
    std::shared_ptr<const lvui::CodeDocumentView> value_;
};
std::vector<std::pair<std::size_t, std::size_t>> boundaries(const std::string& text) {
    std::vector<std::pair<std::size_t, std::size_t>> result{{0, 0}}; std::size_t at = 0, units = 0;
    while (at < text.size()) {
        const auto lead = static_cast<unsigned char>(text[at]);
        const auto bytes = lead < 0x80 ? 1u : lead < 0xe0 ? 2u : lead < 0xf0 ? 3u : 4u;
        at += bytes; units += bytes == 4 ? 2 : 1; result.emplace_back(units, at);
    }
    return result;
}
}
int main() {
    using namespace lvui;
    auto document = take(CodeMemoryDocument::create("before\nA😀B\nafter")); auto original = take(document->snapshot());
    auto composition = take(CodeComposition::create(original, {7, 4}, "한😀\nZ", {1, 2}));
    auto overlay = composition.view();
    require(composition.marked().location == 7 && composition.marked().length == 5 &&
            composition.selection().location == 8 && composition.selection().length == 2 &&
            take(overlay->read({0, overlay->length()}, 100)) == "before\n한😀\nZ\nafter" &&
            take(document->snapshot())->revision() == 0 && take(original->read({7, 4}, 16)) == "A😀B",
            "Composition changed the stored document or projected the wrong text");
    require(!CodeComposition::create(original, {9, 0}, "x", {0, 0}) &&
            !CodeComposition::create(original, {7, 0}, "😀", {1, 0}) &&
            !CodeComposition::create(original, {7, 0}, std::string(65537, 'x'), {0, 0}) &&
            !CodeComposition::create(original, {7, 0}, std::string("x\0y", 3), {0, 0}),
            "Composition accepted invalid Unicode boundaries or unbounded input");
    auto controller = take(CodeEditorController::create(document, document));
    auto committed = take(controller.replace_range(composition.base_range(), composition.text()));
    require(committed.revision == 1 && take(controller.snapshot())->length() == overlay->length() &&
            take(controller.snapshot())->byte_size() == overlay->byte_size(), "Composition commit did not match its projected view");
    auto big = take(CodeMemoryDocument::create(std::string(5 * 1024 * 1024, 'a')));
    auto huge = take(CodeComposition::create(take(big->snapshot()), {10, 5 * 1024 * 1024 - 20}, "é", {1, 0}));
    require(huge.view()->length() == 21 && huge.view()->byte_size() == 22 &&
            take(huge.view()->read({9, 3}, 8)) == "aéa", "Large composition replacement was not bounded correctly");
    auto fallback_source = take(CodeMemoryDocument::create(std::string(65535, 'a') + "😀x"));
    GenericView fallback(take(fallback_source->snapshot()));
    require(take(fallback.range_byte_size({0, fallback.length()})) == 65540,
            "Generic range byte count did not handle a scalar across its read boundary");

    std::mt19937 random(731); const std::vector<std::string> alphabet{"a", "b", "\n", "\r", "한", "😀"};
    for (int iteration = 0; iteration < 350; ++iteration) {
        std::string source, insert;
        for (int i = 0, n = int(random() % 60); i < n; ++i) source += alphabet[random() % alphabet.size()];
        for (int i = 0, n = int(random() % 14); i < n; ++i) insert += alphabet[random() % alphabet.size()];
        const auto points = boundaries(source); auto left = std::size_t(random() % points.size()), right = std::size_t(random() % points.size());
        if (left > right) std::swap(left, right);
        const CodeDocumentRange removed{points[left].first, points[right].first - points[left].first};
        auto base = take(CodeMemoryDocument::create(source));
        auto projected = take(CodeComposition::create(take(base->snapshot()), removed, insert, {0, 0})).view();
        auto expected = take(CodeMemoryDocument::create(source.substr(0, points[left].second) + insert + source.substr(points[right].second)));
        auto reference = take(expected->snapshot());
        require(projected->length() == reference->length() && projected->byte_size() == reference->byte_size() &&
                projected->line_count() == reference->line_count() &&
                take(projected->read({0, projected->length()}, 4096)) == take(reference->read({0, reference->length()}, 4096)),
                "Random composition projection diverged from the equivalent document");
        for (std::size_t line = 0; line < reference->line_count(); ++line)
            require(take(projected->line_start(line)) == take(reference->line_start(line)), "Composition line index diverged");
        for (std::size_t pos = 0; pos <= projected->length(); ++pos) {
            auto actual = projected->read({pos, 0}, 0), check = reference->read({pos, 0}, 0);
            require(bool(actual) == bool(check), "Composition accepted a split Unicode scalar");
        }
        const auto output = take(reference->read({0, reference->length()}, 4096)); const auto ends = boundaries(output);
        for (int sample = 0; sample < 8; ++sample) {
            auto a = std::size_t(random() % ends.size()), b = std::size_t(random() % ends.size()); if (a > b) std::swap(a, b);
            const CodeDocumentRange slice{ends[a].first, ends[b].first - ends[a].first};
            require(take(projected->read(slice, 4096)) == output.substr(ends[a].second, ends[b].second - ends[a].second),
                    "Composition returned an incorrect partial read");
        }
    }
}
