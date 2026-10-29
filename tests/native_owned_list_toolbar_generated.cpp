#include "NativeOwnedListToolbar.generated.h"
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
    if (item == nodes.end()) throw std::runtime_error("Generated list toolbar slot did not mount");
    return take(tree->find(item->id));
}
}
int main() {
    using View = lvui_generated::NativeOwnedListToolbar;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 100})); std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    auto slot = find(tree, "/toolbar.slot0"); good(slot.activate());
    require(scope->backend_0.clicks == 1 && scope->backend_0.index == 0 && scope->backend_0.name == "home",
            "Generated toolbar lost its slot callback");
    scope->backend_0.interactive.set(false); good(slot.activate());
    require(scope->backend_0.clicks == 1, "Reactive toolbar interactive state was ignored");
    scope->backend_0.interactive.set(true); scope->backend_0.icon.set("missing-icon");
    auto graph = scope->native_lifetime.lock();
    require(graph && !take(graph->failures()).empty() && take(slot.action_options()).accessibility_name == "home",
            "Invalid reactive icon replaced the generated toolbar");
    scope->backend_0.icon.set("generalchevronDown");
    require(take(graph->failures()).empty() && take(slot.action_options()).accessibility_name == "generalchevronDown",
            "Generated toolbar did not recover after an icon failure");
    good(slot.activate()); require(scope->backend_0.clicks == 2 && scope->backend_0.name == "generalchevronDown",
            "Recovered toolbar did not deliver its current icon");
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !slot.snapshot(), "Generated list toolbar retained its activation");
    tree->close();
}
