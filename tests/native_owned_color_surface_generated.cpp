#include "NativeOwnedColorSurface.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix) {
    const auto nodes = take(tree->snapshots());
    const auto item = std::ranges::find_if(nodes, [&](const auto& value) { return value.id.ends_with(suffix); });
    if (item == nodes.end()) throw std::runtime_error("Generated color surface did not mount");
    return take(tree->find(item->id));
}
}

int main() {
    using View = lvui_generated::NativeOwnedColorSurface;
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 160}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto plane = find(tree, "/plane"), rail = find(tree, "/rail"), button = find(tree, "/button");
    auto bounds = take(plane.snapshot()).bounds;
    require(take(button.snapshot()).accessibility_description == "#FF0000FF", "Generated color button is not connected to its source");
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = bounds.x + 50, .y = bounds.y + 50}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = bounds.x + 50, .y = bounds.y + 50}));
    require(scope->backend_0.edits == 1 && take(button.snapshot()).accessibility_description == "#804040FF" &&
            take(rail.snapshot()).accessibility_description == "#804040FF",
            "Generated surfaces and button did not share an edited color");
    scope->backend_0.kind.set(lvui::ColorSurfaceKind::wheel);
    auto graph = scope->native_lifetime.lock(); require(bool(graph) && take(graph->failures()).empty(), "Reactive color surface update failed");
    scope->backend_0.source.set(std::shared_ptr<lvui::ColorSurfaceSource>{});
    require(!take(graph->failures()).empty() && take(plane.snapshot()).accessibility_description == "#804040FF",
            "A null surface source replaced the last valid frame");
    scope->backend_0.source.set(scope->backend_0.owner);
    require(take(graph->failures()).empty(), "The generated surface did not recover its source");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !plane.snapshot(), "Generated color surface retained its activation");
    tree->close();
}
