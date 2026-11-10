#include "NativePanelColors.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix) {
    auto nodes = take(tree->snapshots());
    auto found = std::ranges::find_if(nodes, [&](const auto& item) { return item.id.ends_with(suffix); });
    if (found == nodes.end()) throw std::runtime_error("Panel was not mounted");
    return take(tree->find(found->id));
}
ii::gui::Panel panel(const ii::gui::CanvasFrame& frame, ii::gui::Rect bounds, float stroke) {
    const auto find = [&](const auto& panels) -> std::optional<ii::gui::Panel> {
        auto found = std::ranges::find_if(panels, [&](const auto& item) {
            return item.bounds == bounds && item.stroke_width == stroke;
        });
        return found == panels.end() ? std::nullopt : std::optional{*found};
    };
    if (auto found = find(frame.panels)) return *found;
    for (const auto& layer : frame.overlays) if (auto found = find(layer.panels)) return *found;
    throw std::runtime_error("Panel drawing is missing from the canvas and overlay layers");
}
}
int main() {
    using View = lvui_generated::NativePanelColors;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {500, 400})); std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto fixed = find(tree, "/staticPanel"), dynamic = find(tree, "/dynamicPanel");
    auto frame = take(tree->drawing_snapshot({500, 400}));
    auto fixed_bounds = take(fixed.snapshot()).bounds, dynamic_bounds = take(dynamic.snapshot()).bounds;
    require(std::abs(panel(frame, fixed_bounds, 0).top.red - .2158605) < .0001 &&
            panel(frame, fixed_bounds, 1).radius == 4 &&
            std::abs(panel(frame, dynamic_bounds, 0).top.red - .21404114) < .0001 &&
            panel(frame, dynamic_bounds, 2).radius == 4, "Static or dynamic panel changed its rendered style");
    scope->backend_0.color.set({1, 0, 0, 1}); frame = take(tree->drawing_snapshot({500, 400}));
    require(panel(frame, dynamic_bounds, 0).top.red == 1 && panel(frame, dynamic_bounds, 0).top.green == 0,
            "Panel color did not update reactively");
    auto graph = scope->native_lifetime.lock(); require(bool(graph), "Panel binding graph is unavailable");
    scope->backend_0.width.set(-1); frame = take(tree->drawing_snapshot({500, 400}));
    require(!take(graph->failures()).empty() && panel(frame, dynamic_bounds, 2).radius == 4,
            "Invalid panel options replaced the prior drawing");
    scope->backend_0.width.set(1); frame = take(tree->drawing_snapshot({500, 400}));
    require(take(graph->failures()).empty() && panel(frame, dynamic_bounds, 1).radius == 4,
            "Panel did not recover after invalid options");
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !fixed.snapshot() && !dynamic.snapshot(), "Panel binding survived retirement");
    tree->close();
}
