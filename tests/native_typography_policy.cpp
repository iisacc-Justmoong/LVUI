#include "NativeTypographyPolicy.generated.h"
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
unsigned weight(const ii::gui::CanvasFrame& frame, std::string_view text) {
    for (const auto& item : frame.texts) if (item.text == text) return item.font_weight;
    for (const auto& layer : frame.overlays)
        for (const auto& item : layer.texts) if (item.text == text) return item.font_weight;
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
        auto host = take(builder.content_host(root, "generatedHost", {300, 50}));
        return host.set_content([](WidgetBuilder& child) { return View::build(child); });
    }));
    tree->commit_publication();
    const auto frame = take(tree->drawing_snapshot({640, 480}));
    if (weight(frame, "Policy inherited") != 700 || weight(frame, "Generated typography") != 700 ||
        weight(frame, "Explicit weight") != 400)
        throw std::runtime_error("Generated LVUI typography did not reach the retained iiGUI tree");
    tree->close();
}
