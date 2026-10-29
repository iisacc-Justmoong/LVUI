#include "NativeOwnedListFooter.generated.h"
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
    if (item == nodes.end()) throw std::runtime_error("Generated footer slot did not mount"); return take(tree->find(item->id));
}
}
int main() {
    using View = lvui_generated::NativeOwnedListFooter;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {200, 80})); std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto first = find(tree, "/footer.slot0"); good(first.activate());
    require(scope->backend_0.clicks == 1 && scope->backend_0.index == 0 && scope->backend_0.name == "home",
            "Generated list footer lost its action or slot data");
    scope->backend_0.interactive.set(false); good(first.activate()); require(scope->backend_0.clicks == 1, "Reactive footer disable was ignored");
    scope->backend_0.interactive.set(true);
    scope->backend_0.first.set(std::make_shared<const lvui::ListFooterSlot>(lvui::ListFooterSlot{.icon_name="missing-icon"}));
    auto graph = scope->native_lifetime.lock();
    require(graph && !take(graph->failures()).empty() && take(first.action_options()).accessibility_name == "Home",
            "An invalid reactive footer slot replaced the working slot");
    scope->backend_0.first.set(std::make_shared<const lvui::ListFooterSlot>(lvui::ListFooterSlot{.icon_name="nodesfolder", .accessibility_name="Folder"}));
    require(take(graph->failures()).empty() && take(first.action_options()).accessibility_name == "Folder", "Footer did not recover after invalid slot input");
    good(first.activate()); require(scope->backend_0.clicks == 2 && scope->backend_0.name == "nodesfolder", "Recovered footer delivered stale slot data");
    scope->backend_0.first.set({}); good(first.activate());
    require(!take(first.interaction()).visible && scope->backend_0.clicks == 2, "A missing footer slot remained visible or actionable");
    scope->backend_0.first.set(std::make_shared<const lvui::ListFooterSlot>(lvui::ListFooterSlot{.icon_name="home"}));
    require(take(first.interaction()).visible, "A restored footer slot did not reappear");
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !first.snapshot(), "Generated footer retained its activation"); tree->close();
}
