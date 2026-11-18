#include "NativeOwnedAlert.generated.h"
#include <iiGUI/SrgbColor.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
const ii::gui::TextRun& text_run(const ii::gui::CanvasFrame& frame, std::string_view text) {
    for (const auto& run : frame.texts) if (run.text == text) return run;
    for (const auto& layer : frame.overlays) for (const auto& run : layer.texts) if (run.text == text) return run;
    throw std::runtime_error("Generated Alert text was not drawn");
}
bool near(ii::gui::Color a,ii::gui::Color b) {
    return std::abs(a.red-b.red)<1e-6 && std::abs(a.green-b.green)<1e-6 &&
           std::abs(a.blue-b.blue)<1e-6 && std::abs(a.alpha-b.alpha)<1e-6;
}
}

int main() {
    using View = lvui_generated::NativeOwnedAlert;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {800, 600}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto nodes = take(tree->snapshots());
    const auto found = std::ranges::find_if(nodes, [](const auto& item) { return item.id.ends_with("/alert"); });
    require(found != nodes.end(), "Generated Alert did not mount");
    auto alert = take(tree->find(found->id));
    (void)take(tree->drawing_snapshot({800, 600})); good(alert.open_at(0, 0));
    const auto frame=take(tree->drawing_snapshot({800, 600}));
    const auto& title=text_run(frame,"Initial");const auto& description=text_run(frame,"Details");
    require(title.font_size==26 && title.line_height==26 && title.font_weight==700 &&
            near(title.color,ii::gui::srgb_to_linear(ii::gui::WidgetColor{244./255,245./255,247./255,1})) &&
            description.font_size==13 && description.line_height==13 && description.font_weight==500 &&
            near(description.color,ii::gui::srgb_to_linear(ii::gui::WidgetColor{214./255,217./255,223./255,1})),
            "Generated Alert did not use its Figma material and typography tokens");
    auto action = take(alert.popup_content("alert.action-0")); good(action.activate());
    require(scope->backend_0.events == std::vector<std::string>{"method", "clicked"},
            "Generated Alert action method and callback did not execute in order");
    scope->backend_0.title.set("Updated"); scope->backend_0.secondaryEnabled.set(false);
    require(take(take(alert.popup_content("alert.title")).snapshot()).text == "Updated" &&
            !take(take(alert.popup_content("alert.action-1")).snapshot()).enabled,
            "Generated Alert did not update reactively");
    auto graph = scope->native_lifetime.lock(); require(bool(graph), "Generated Alert graph is unavailable");
    scope->backend_0.count.set(4); scope->backend_0.title.set("Deferred");
    require(!take(graph->failures()).empty() && take(take(alert.popup_content("alert.title")).snapshot()).text == "Updated",
            "Invalid Alert inputs changed the retained component");
    scope->backend_0.count.set(3);
    require(take(graph->failures()).empty() && take(take(alert.popup_content("alert.title")).snapshot()).text == "Deferred" &&
            take(take(alert.popup_content("alert.action-2")).snapshot()).visible,
            "Generated Alert did not recover after an invalid candidate");
    good(alert.cancel_popup());
    require(scope->backend_0.events.back() == "dismissed", "Generated Alert dismissal was not delivered");
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !alert.snapshot(), "Generated Alert retained its activation"); tree->close();
}
