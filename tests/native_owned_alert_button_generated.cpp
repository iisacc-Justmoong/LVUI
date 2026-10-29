#include "NativeOwnedAlertButton.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
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
}

int main() {
    using View = lvui_generated::NativeOwnedAlertButton;
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 200}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto nodes = take(tree->snapshots());
    const auto found = std::ranges::find_if(nodes, [](const auto& item) { return item.id.ends_with("/alert"); });
    require(found != nodes.end(), "Generated AlertButton did not mount");
    auto button = take(tree->find(found->id));
    (void)take(tree->drawing_snapshot({300, 200}));
    auto snapshot = take(button.snapshot());
    const auto x = snapshot.bounds.x + snapshot.bounds.width / 2, y = snapshot.bounds.y + snapshot.bounds.height / 2;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = x, .y = y}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = x, .y = y}));
    require(scope->backend_0.events == std::vector<std::string>{"pressed", "released", "method", "clicked"} &&
            scope->backend_0.source_id == snapshot.id, "Generated AlertButton callbacks or event source failed");
    scope->backend_0.text.set("Save"); scope->backend_0.tone.set(lvui::ButtonTone::primary);
    require(take(button.snapshot()).text == "Save", "Generated AlertButton did not update reactively");
    auto graph = scope->native_lifetime.lock(); require(bool(graph), "Generated AlertButton bindings are unavailable");
    scope->backend_0.height.set(0);
    scope->backend_0.text.set("Retry");
    const auto failures = take(graph->failures());
    const auto rejected = take(button.snapshot());
    if (failures.empty() || rejected.bounds.height != 56 || rejected.text != "Save") {
        throw std::runtime_error("An invalid AlertButton input replaced its current state: failures=" +
            std::to_string(failures.size()) + ", height=" + std::to_string(rejected.bounds.height));
    }
    scope->backend_0.height.set(40);
    require(take(graph->failures()).empty() && take(button.snapshot()).bounds.height == 40 &&
            take(button.snapshot()).text == "Retry",
            "Generated AlertButton did not recover its valid height");
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !button.snapshot(), "Generated AlertButton retained its activation"); tree->close();
}
