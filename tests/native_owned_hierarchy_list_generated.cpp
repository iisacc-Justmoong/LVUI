#include "NativeOwnedHierarchyList.generated.h"
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
template<class T> T take(lvui::HierarchyResult<T> value) {
    if (!value) throw std::runtime_error(value.error().message); return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view name) {
    const auto nodes = take(tree->snapshots());
    const auto item = std::ranges::find_if(nodes, [&](const auto& value) { return value.accessibility_name == name; });
    if (item == nodes.end()) throw std::runtime_error("Generated hierarchy row did not mount");
    return take(tree->find(item->id));
}
}

int main() {
    using View = lvui_generated::NativeOwnedHierarchyList;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {400, 140}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    (void)take(tree->snapshots());
    auto row = find(tree, "Root"), chevron = find(tree, "Expand Root");
    good(chevron.activate());
    require(scope->backend_0.expansions == 1 && scope->backend_0.activations == 1 &&
            scope->backend_0.active_key == "root" && scope->backend_0.active_id == 42 && scope->backend_0.active_index == 0 &&
            find(tree, "Child").snapshot() && take(scope->backend_0.source->snapshot())->revision == 0,
            "Generated hierarchy expansion, activation or source isolation failed");
    auto changed = take(scope->backend_0.source->snapshot())->entries; changed[0].label = "Renamed";
    const auto revision = take(scope->backend_0.source->replace(changed, 0)); scope->backend_0.revision.set(std::int64_t(revision));
    auto graph = scope->native_lifetime.lock();
    require(graph && take(graph->failures()).empty() && take(row.action_options()).accessibility_name == "Renamed",
            "Generated hierarchy list did not apply a source revision");
    scope->backend_0.width.set(0);
    require(!take(graph->failures()).empty() && take(row.action_options()).accessibility_name == "Renamed",
            "Invalid reactive list configuration replaced the last valid view");
    scope->backend_0.width.set(220);
    require(take(graph->failures()).empty(), "Generated hierarchy list did not recover from invalid configuration");
    good(row.focus()); scope->backend_0.keyboard.set(false);
    good(tree->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::down}));
    require(take(scope->backend_0.controller.get()->selected_key()) == "root", "Reactive keyboard disable was ignored");
    scope->backend_0.keyboard.set(true);
    good(tree->input({.kind=ii::gui::WidgetInputKind::key_down, .key=ii::gui::WidgetKey::down}));
    require(take(scope->backend_0.controller.get()->selected_key()) == "child" && scope->backend_0.active_key == "child",
            "Reactive keyboard enable did not navigate the hierarchy");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down, .x=10, .y=30, .buttons=1}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move, .x=10, .y=58, .buttons=1}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up, .x=10, .y=58, .buttons=1}));
    auto moved = take(scope->backend_0.source->snapshot());
    require(scope->backend_0.moves == 1 && moved->revision == 2 && moved->entries.back().key == "child" &&
            moved->entries.back().parent.empty() && take(graph->failures()).empty(),
            "Generated hierarchy did not commit and publish its owned drag move");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !row.snapshot(), "Generated hierarchy list retained its activation");
    tree->close();
}
