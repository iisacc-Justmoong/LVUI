#include "NativeOwnedListItem.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix) {
    const auto nodes = take(tree->snapshots());
    const auto found = std::ranges::find_if(nodes, [&](const auto& value) { return value.id.ends_with(suffix); });
    if (found == nodes.end()) throw std::runtime_error("Generated list item did not mount");
    return take(tree->find(found->id));
}
}
int main() {
    using View = lvui_generated::NativeOwnedListItem;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {400,100})); std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto item = find(tree, "/item"); good(item.activate());
    require(scope->backend_0.clicks == 1 && take(item.action_options()).accessibility_name == "Camera", "Generated item lost its callback or label");
    scope->backend_0.label.set("Light"); scope->backend_0.description.set("Shared light"); scope->backend_0.selected.set(true);
    require(take(item.action_options()).accessibility_name == "Light" && take(item.action_options()).accessibility_description == "Shared light",
            "Generated list item did not react to its model");
    scope->backend_0.enabled.set(false); good(item.activate()); require(scope->backend_0.clicks == 1, "Reactive item disable was ignored");
    scope->backend_0.enabled.set(true); auto graph = scope->native_lifetime.lock();
    scope->backend_0.width.set(-1); require(graph && !take(graph->failures()).empty(), "Invalid reactive width was accepted");
    scope->backend_0.width.set(160); require(take(graph->failures()).empty(), "List item did not recover after invalid width");
    scope->backend_0.visible.set(false); require(!take(item.interaction()).visible && !item.activate(), "Reactive item visibility was ignored");
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !item.snapshot(), "Generated item retained its activation"); tree->close();
}
