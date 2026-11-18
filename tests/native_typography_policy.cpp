#include "NativeTypographyPolicy.generated.h"
#include <iiGUI/SrgbColor.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
template<class T> T take(ii::core::Result<T> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
const ii::gui::TextRun& text_run(const ii::gui::CanvasFrame& frame, std::string_view text) {
    for (const auto& item : frame.texts) if (item.text == text) return item;
    for (const auto& layer : frame.overlays)
        for (const auto& item : layer.texts) if (item.text == text) return item;
    throw std::runtime_error("Typography test text was not drawn");
}
}

int main() {
    using namespace ii::gui;
    using View = lvui_generated::NativeTypographyPolicy;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); }, View::appearance());
    good(tree->construct([&](WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = take(builder.vstack({}, "root", {.alignment = StackAlignment::leading}));
        take(builder.label(root, "inherited", {.text = "Policy inherited", .font_size = 19.5, .role = TextRole::body}));
        take(builder.label(root, "explicit", {.text = "Explicit weight", .font_size = 19.5,
            .role = TextRole::body, .font_weight = 400}));
        auto host = take(builder.content_host(root, "generatedHost", {480, 320}));
        return host.set_content([](WidgetBuilder& child) { return View::build(child); });
    }));
    tree->commit_publication();
    const auto frame = take(tree->drawing_snapshot({640, 480}));
    if (text_run(frame, "Policy inherited").font_weight != 700 || text_run(frame, "Generated typography").font_weight != 700 ||
        text_run(frame, "Explicit weight").font_weight != 400)
        throw std::runtime_error("Generated LVUI typography did not reach the retained iiGUI tree");
    const auto expected_body = srgb_to_linear(WidgetColor{.125, .25, .5, .75});
    const auto expected_caption = srgb_to_linear(WidgetColor{.25, .5, .75, .5});
    for (const auto text : {"Themed item", "Themed composite"}) {
        const auto& item = text_run(frame, text);
        if (item.font_size != 19.5f || item.line_height != 20 || item.font_weight != 700 || item.color != expected_body)
            throw std::runtime_error("Generated body role did not reach a list item");
    }
    for (const auto text : {"Themed detail", "Composite detail"}) {
        const auto& item = text_run(frame, text);
        if (item.font_size != 12 || item.line_height != 12 || item.font_weight != 600 || item.color != expected_caption)
            throw std::runtime_error("Generated description role did not reach a list item");
    }
    tree->close();
}
