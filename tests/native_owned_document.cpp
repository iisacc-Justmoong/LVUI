#include <LVUI/NativeDocument.h>
#include <algorithm>
#include <concepts>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
lvui::Document parse(std::string_view source) {
    auto value = lvui::parse(source);
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
const ii::gui::Rect& box(const lvui::native::DocumentLayout& layout, std::string_view id) {
    const auto found = std::ranges::find_if(layout.elements, [&](const auto& element) { return element.id == id; });
    if (found == layout.elements.end()) throw std::runtime_error("Missing document element");
    return found->bounds;
}
static_assert(requires(ii::gui::Host& host, const lvui::Document& document) {
    { host.set_view(document) } -> std::same_as<ii::core::Result<void>>;
});
}
int main() {
    auto document = parse("<view style='padding:20px; gap:10px'><view id='row' style='flex-direction:row; gap:10px; height:80px'><view id='fixed' style='width:100px'/><view id='flex'/><view id='percent' style='width:25%'/></view><p id='text'>A long piece of native text that wraps across several lines.</p></view>");
    auto wide = lvui::native::layout_document(document, {640, 420});
    auto narrow = lvui::native::layout_document(document, {300, 420});
    require(wide && narrow, "Valid document could not be laid out");
    require(box(*wide, "row") == ii::gui::Rect{20, 20, 600, 80} &&
            box(*wide, "fixed").width == 100 && box(*wide, "flex").width == 330 &&
            box(*wide, "percent").width == 150, "LVUI document row geometry changed");
    require(box(*narrow, "text").height >= box(*wide, "text").height,
            "Document text did not reflow with its viewport");
    require(!lvui::native::layout_document(document, {0, 420}) &&
            !lvui::native::layout_document(document, {std::numeric_limits<double>::infinity(), 1}),
            "Invalid viewport was accepted");
    require(!lvui::native::layout_document(parse("<view style='padding:60px'/>"), {100, 50}),
            "Invalid layout was accepted");
}
