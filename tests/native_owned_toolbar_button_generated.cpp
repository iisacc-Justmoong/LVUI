#include "NativeOwnedToolbarButton.generated.h"
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
    if (item == nodes.end()) throw std::runtime_error("Generated toolbar button did not mount");
    return take(tree->find(item->id));
}
}
int main() {
    using View = lvui_generated::NativeOwnedToolbarButton;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {200, 80})); std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto button = find(tree, "/tool"); good(button.activate());
    require(scope->backend_0.clicks == 1 && scope->backend_0.ids == std::vector<lvui::ToolbarButtonId>{std::string("open")},
            "Generated toolbar button lost its callback or identity");
    scope->backend_0.enabled.set(false); good(button.activate()); require(scope->backend_0.clicks == 1, "Reactive disable was ignored");
    scope->backend_0.enabled.set(true); scope->backend_0.icon.set("missing-icon"); auto graph = scope->native_lifetime.lock();
    require(graph && !take(graph->failures()).empty() && take(button.action_options()).accessibility_name == "Open",
            "An invalid reactive icon replaced the toolbar button");
    scope->backend_0.icon.set("generalchevronDown"); scope->backend_0.text.set("Next");
    scope->backend_0.identity.set(lvui::ToolbarButtonId{std::int64_t(42)});
    require(take(graph->failures()).empty() && take(button.action_options()).accessibility_name == "Next", "Toolbar button did not recover after invalid input");
    good(button.activate()); require(scope->backend_0.clicks == 2 && std::get<std::int64_t>(scope->backend_0.ids.back()) == 42,
            "Generated toolbar button did not use its updated identity");
    scope->backend_0.visible.set(false); require(!take(button.interaction()).visible && !button.activate(), "Reactive toolbar visibility was ignored");
    scope->backend_0.visible.set(true); scope->backend_0.width.set(-1); require(!take(graph->failures()).empty(), "Invalid width was accepted");
    scope->backend_0.width.set(100); require(take(graph->failures()).empty(), "Toolbar button did not recover after invalid width");
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !button.snapshot(), "Generated toolbar button retained its activation"); tree->close();
}
