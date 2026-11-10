#include "NativeQueryConnections.generated.h"
#include "NativeQueryScene.generated.h"
#include <LVUI/NativeQueryConnections.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> result) {
    if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); }
    return std::move(*result);
}
void good(ii::core::Result<void> result) {
    if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); }
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix) {
    const auto nodes = take(tree->snapshots());
    const auto found = std::ranges::find_if(nodes, [&](const auto& node) { return node.id.ends_with(suffix); });
    if (found == nodes.end()) throw std::runtime_error("Missing query fixture widget");
    return take(tree->find(found->id));
}
void scene() {
    using View = lvui_generated::NativeQueryScene;
    {
        ii::runtime::Application app;
        auto registry = take(lvui::native::QueryConnections::create(app.event_loop()));
        auto factories = lvui::native::BackendFactories{}.with_query_connections(registry);
        good(View::configure_application(app, {}, factories));
        check(take(registry->close()), "Application query policy did not close before activation");
    }
    ii::runtime::EventLoop loop(64);
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events = [&]() -> ii::core::Result<ii::runtime::EventLoop*> { return &loop; };
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); }, ii::gui::WidgetAppearance{}, hooks);
    auto host = take(tree->content_host({}, "scene-host", {320, 200}));
    auto registry = take(lvui::native::QueryConnections::create(loop));
    auto factories = lvui::native::BackendFactories{}.with_query_connections(registry);
    auto application = View::make_application_owner({}, factories);
    good(application->activate());
    good(host.set_content([&](auto& builder) { return View::build_with_application(builder, {}, application); }));
    (void)application->scope()->backend_0.events.publish({8, false});
    loop.poll({64, 1s});
    check(take(find(tree, "/count").snapshot()).text == "8", "Application Scene lost its query connection or local state");
    good(host.clear_content());
    good(application->retire());
    check(take(registry->close()), "Scene query owner did not retire");
    tree->close();
}
}

int main() {
    using View = lvui_generated::NativeQueryConnections;
    ii::runtime::EventLoop loop(64), other_loop(64);
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events = [&]() -> ii::core::Result<ii::runtime::EventLoop*> { return &loop; };
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); }, ii::gui::WidgetAppearance{}, hooks);
    auto host = take(tree->content_host({}, "host", {320, 200}));
    auto registry = take(lvui::native::QueryConnections::create(loop));
    auto other = take(lvui::native::QueryConnections::create(other_loop));
    auto factories = lvui::native::BackendFactories{}.with_query_connections(registry);
    auto wrong_factories = lvui::native::BackendFactories{}.with_query_connections(other);
    check(!host.set_content([&](auto& builder) { return View::build(builder, {}, {}); }), "Missing registry was accepted");
    check(!host.set_content([&](auto& builder) { return View::build(builder, {}, {}, nullptr, wrong_factories); }), "Wrong UI executor was accepted");
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, {}, &scope, factories); }));
    auto label = find(tree, "/count");
    auto snapshots = take(registry->stats());
    check(registry->size() == 4 && snapshots.size() == 4 && snapshots[0].stats.capacity == 2 &&
          take(label.snapshot()).text == "0", "Generated connections or queue options failed to mount");
    good(find(tree, "/increment").activate());
    check(take(label.snapshot()).text == "1", "Button did not update the View session variable");
    std::thread producer([&] { (void)scope->backend_0.events.publish({2, false}); });
    producer.join();
    check(take(label.snapshot()).text == "1" && scope->backend_0.received.empty(), "Query delivery escaped the UI loop");
    loop.poll({64, 1s});
    check(take(label.snapshot()).text == "2" && scope->backend_0.received == std::vector<std::int64_t>{2},
          "Generated event block or ViewModel method did not receive its payload");
    check(scope->backend_0.refreshes == 1 && take(find(tree, "/calls").snapshot()).text == "1",
          "No-argument method or event callback did not receive a query notification");
    (void)scope->backend_0.events.publish({4, false});
    scope->backend_0.selection.set(1);
    (void)scope->backend_0.events.publish({5, true});
    loop.poll({64, 1s});
    check(take(label.snapshot()).text == "5" && scope->backend_0.received == std::vector<std::int64_t>({2, 5}),
          "Generated query did not update its selection or retire its conditional receiver");
    loop.poll({64, 1s});
    check(registry->size() == 3, "Conditional receiver was not retired after its callback");
    (void)scope->backend_0.events.publish({7, false});
    loop.poll({64, 1s});
    check(take(label.snapshot()).text == "7" && scope->backend_0.received == std::vector<std::int64_t>({2, 5}),
          "Removed conditional receiver accepted a late event");
    scope->backend_0.visible.set(true);
    check(registry->size() == 4, "Conditional query did not reconnect");
    (void)scope->backend_0.events.publish({9, false});
    loop.poll({64, 1s});
    check(scope->backend_0.received == std::vector<std::int64_t>({2, 5, 9}), "Recreated receiver lost its query");
    (void)scope->backend_0.events.publish({11, false});
    (void)scope->backend_0.events.publish({13, false});
    (void)scope->backend_0.events.publish({15, false});
    snapshots = take(registry->stats());
    check(snapshots[0].stats.dropped == 1 && snapshots[0].stats.pending == 2,
          "Generated overflow policy did not bound the queue");
    loop.poll({64, 1s});
    check(take(label.snapshot()).text == "15" && scope->backend_0.received == std::vector<std::int64_t>({2, 5, 9, 11, 13, 15}),
          "Independent receivers did not keep their own bounded delivery policies");
    check(scope->backend_0.refreshes == 7 && take(find(tree, "/calls").snapshot()).text == "7",
          "No-argument receivers lost selected notifications");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content()); scope.reset();
    loop.poll({64, 1s});
    check(weak.expired() && take(registry->close()) && take(other->close()), "Retired View retained query owners");
    tree->close();
    scene();
}
