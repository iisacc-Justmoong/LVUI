#include "NativeOwnedHierarchyToolbar.generated.h"
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
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view name) {
    const auto nodes = take(tree->snapshots());
    const auto item = std::ranges::find_if(nodes, [&](const auto& value) { return value.accessibility_name == name; });
    if (item == nodes.end()) throw std::runtime_error("Generated hierarchy toolbar item did not mount"); return take(tree->find(item->id));
}
}
int main() {
    using View = lvui_generated::NativeOwnedHierarchyToolbar;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 80})); std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto home = find(tree, "Home tool"), add = find(tree, "Add tool"); const auto home_id = take(home.snapshot()).id;
    good(add.activate());
    require(scope->backend_0.changes == 1 && scope->backend_0.actions == 1 && scope->backend_0.last &&
            scope->backend_0.last->id == lvui::ToolbarButtonId{std::string("add")},
            "Generated toolbar lost its controlled selection or action callback");
    auto items = scope->backend_0.items.get(); std::swap(items[0], items[1]); scope->backend_0.items.set(items);
    require(take(home.snapshot()).id == home_id && scope->backend_0.actions == 1, "Generated toolbar rebuilt a surviving keyed item");
    items[0].icon_name = "missing-icon"; scope->backend_0.items.set(items); auto graph = scope->native_lifetime.lock();
    require(graph && !take(graph->failures()).empty() && take(add.action_options()).accessibility_name == "Add tool",
            "An invalid toolbar icon replaced the last good state");
    items[0].icon_name = "generaladd"; scope->backend_0.items.set(items);
    require(take(graph->failures()).empty(), "Generated toolbar did not recover after invalid item input");
    scope->backend_0.active.set(lvui::ToolbarButtonId{std::string("missing")});
    require(!take(graph->failures()).empty(), "An unknown controlled toolbar selection was accepted");
    scope->backend_0.active.set(lvui::ToolbarButtonId{std::string("add")}); require(take(graph->failures()).empty(), "Toolbar did not recover after invalid selection");
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !home.snapshot(), "Generated hierarchy toolbar retained its activation"); tree->close();
}
