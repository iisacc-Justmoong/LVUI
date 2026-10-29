#include "NativeOwnedAlert.generated.h"
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
    using View = lvui_generated::NativeOwnedAlert;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {800, 600}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto nodes = take(tree->snapshots());
    const auto found = std::ranges::find_if(nodes, [](const auto& item) { return item.id.ends_with("/alert"); });
    require(found != nodes.end(), "Generated Alert did not mount");
    auto alert = take(tree->find(found->id));
    (void)take(tree->drawing_snapshot({800, 600})); good(alert.open_at(0, 0));
    auto action = take(alert.popup_content("alert.action-0")); good(action.activate());
    require(scope->backend_0.events == std::vector<std::string>{"method", "clicked"},
            "Generated Alert action method and callback did not execute in order");
    scope->backend_0.title.set("Updated"); scope->backend_0.secondaryEnabled.set(false);
    require(take(take(alert.popup_content("alert.title")).snapshot()).text == "Updated" &&
            !take(take(alert.popup_content("alert.action-1")).snapshot()).enabled,
            "Generated Alert did not update reactively");
    auto graph = scope->native_lifetime.lock(); require(bool(graph), "Generated Alert graph is unavailable");
    scope->backend_0.count.set(4); scope->backend_0.title.set("Deferred");
    require(!take(graph->failures()).empty() && take(take(alert.popup_content("alert.title")).snapshot()).text == "Updated",
            "Invalid Alert inputs changed the retained component");
    scope->backend_0.count.set(3);
    require(take(graph->failures()).empty() && take(take(alert.popup_content("alert.title")).snapshot()).text == "Deferred" &&
            take(take(alert.popup_content("alert.action-2")).snapshot()).visible,
            "Generated Alert did not recover after an invalid candidate");
    good(alert.cancel_popup());
    require(scope->backend_0.events.back() == "dismissed", "Generated Alert dismissal was not delivered");
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !alert.snapshot(), "Generated Alert retained its activation"); tree->close();
}
