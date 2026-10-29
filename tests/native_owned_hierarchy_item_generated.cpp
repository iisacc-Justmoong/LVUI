#include "NativeOwnedHierarchyItem.generated.h"
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
    const auto item = std::ranges::find_if(nodes, [&](const auto& value) { return value.id.ends_with(suffix); });
    if (item == nodes.end()) throw std::runtime_error("Generated hierarchy item did not mount");
    return take(tree->find(item->id));
}
}

int main() {
    using View = lvui_generated::NativeOwnedHierarchyItem;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {400, 120}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto row = find(tree, "/item.row"), chevron = find(tree, "/item.chevron");
    require(take(row.action_options()).accessibility_name == "Camera" &&
            take(row.action_options()).accessibility_description == "Count 12", "Generated hierarchy row lost its values");
    good(chevron.activate());
    require(scope->backend_0.expansions == 1 && scope->backend_0.activations == 1 && scope->backend_0.expanded.get() &&
            take(chevron.action_options()).accessibility_name == "Collapse Camera", "Generated hierarchy events or reactive echo failed");
    scope->backend_0.label.set("Layer"); scope->backend_0.count.set(0);
    auto graph = scope->native_lifetime.lock();
    require(graph && take(graph->failures()).empty() && take(row.action_options()).accessibility_name == "Layer" &&
            take(row.action_options()).accessibility_description == "Count 0", "Hierarchy row did not apply reactive inputs");
    scope->backend_0.count.set(-2);
    require(!take(graph->failures()).empty() && take(row.action_options()).accessibility_description == "Count 0",
            "Invalid reactive hierarchy data replaced the last valid row");
    scope->backend_0.count.set(4); scope->backend_0.children.set(false);
    require(take(graph->failures()).empty() && !take(chevron.interaction()).visible, "Hierarchy row did not recover or hide its chevron");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !row.snapshot(), "Generated hierarchy row retained its activation");
    tree->close();
}
