#include "LVUINativeDynamicWindowsFixture.generated.h"
#include "LVUINativeDynamicWindowsMarkupFixture.generated.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using View = lvui_generated::LVUINativeDynamicWindowsFixture;
using Markup = lvui_generated::LVUINativeDynamicWindowsMarkupFixture;
using State = ii::gui::detail::WidgetState;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(ii::core::Result<void> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
template<class T> T take(ii::core::Result<T> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
    return std::move(*value);
}
ii::gui::Widget find(const std::shared_ptr<State>& tree, std::string_view name) {
    for (const auto& node : take(tree->snapshots()))
        if (node.id.ends_with("/" + std::string(name))) return take(tree->find(node.id));
    throw std::runtime_error("Generated dynamic-window control was not found");
}
struct Window final { std::string name; bool closed = false; };

std::shared_ptr<lvui::native::SceneRegistry> make_registry(ii::runtime::EventLoop& loop,
        std::vector<std::shared_ptr<Window>>& opened, int& errors) {
    auto registry = std::make_shared<lvui::native::SceneRegistry>(
        [sender = loop.sender()](auto task) -> ii::core::Result<void> {
            if (!sender.post(std::move(task))) return ii::core::fail(ii::core::ErrorCode::closed, "Scene queue rejected request");
            return {};
        }, [&](const auto&) { ++errors; });
    for (std::string name : {"tools", "palette"})
        good(registry->define(name, [name, &opened](bool initial) -> ii::core::Result<lvui::native::SceneInstance> {
            if (initial) return ii::core::fail(ii::core::ErrorCode::invalid_argument, "Unexpected initial window");
            auto window = std::make_shared<Window>(name); opened.push_back(window);
            return lvui::native::SceneInstance{window,
                [window]() -> ii::core::Result<bool> { return window->closed; },
                [window]() -> ii::core::Result<void> { window->closed = true; return {}; }};
        }));
    return registry;
}

void dynamic_targets_follow_event_time_and_scope_lifetime() {
    ii::runtime::EventLoop loop; int errors = 0;
    std::vector<std::shared_ptr<Window>> opened;
    auto registry = make_registry(loop, opened, errors);
    auto owner = View::make_application_owner();
    good(owner->bind_scene_registry(registry)); good(owner->activate());
    auto scope = owner->scope(); check(bool(scope), "Application scope was not activated");
    auto graph = scope->native_lifetime.lock(); check(bool(graph), "Application graph was not activated");
    auto& backend = scope->backend_0;
    auto tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "main_host", {640, 480}));
    good(host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 0); }));
    good(owner->notify_ready());
    check(backend.queries == 0, "A window target method ran while the view was built");

    good(graph->begin_batch());
    backend.target.set("palette");
    auto nested = find(tree, "nested"); good(nested.activate());
    check(opened.empty(), "A window opened inside its event callback");
    backend.target.set("tools");
    good(graph->end_batch()); loop.poll();
    check(opened.size() == 1 && opened[0]->name == "palette",
          "Nested derived target was stale or changed after admission");

    backend.target.set("missing");
    bool unknown = false;
    try { (void)find(tree, "open").activate(); }
    catch (const std::runtime_error& error) { unknown = std::string_view(error.what()).find("Unknown scene") != std::string_view::npos; }
    loop.poll();
    check(unknown && opened.size() == 1 && errors == 0, "Unknown dynamic target was silently admitted");
    backend.target.set("tools"); good(find(tree, "open").activate()); loop.poll();
    check(opened.size() == 2 && opened[1]->name == "tools", "A valid target failed after an unknown target");

    good(find(tree, "close").activate());
    backend.target.set("palette"); loop.poll();
    check(opened[1]->closed && !opened[0]->closed, "A queued close followed a later target value");
    check(backend.queries == 0, "Changing the target invoked its method");
    good(find(tree, "query").activate());
    check(backend.queries == 1, "Target method was not evaluated exactly once on the event");
    loop.poll(); check(opened.size() == 2, "Opening an existing target created a duplicate");
    backend.target.set("tools"); good(find(tree, "toggle").activate()); loop.poll();
    check(opened.size() == 3 && opened[2]->name == "tools", "An event with an argument did not open its target");

    backend.target.set("palette"); good(find(tree, "close").activate()); loop.poll();
    check(opened[0]->closed, "The palette was not closed before the retirement check");
    auto query = find(tree, "query");
    backend.during_choose = [&] { good(host.clear_content()); };
    good(query.activate()); loop.poll();
    check(opened.size() == 3 && backend.queries == 2,
          "A target evaluation admitted a window request after its view retired");
    check(!nested.activate(), "A retired window control still accepted input");
    good(host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 0); }));
    auto convert = find(tree, "convert");
    backend.view_target.during_conversion = [&] { good(host.clear_content()); };
    good(convert.activate()); loop.poll();
    check(opened.size() == 3, "A target conversion admitted a window request after its view retired");
    good(registry->shutdown()); good(owner->retire()); tree->close();
    check(errors == 0, "Dynamic window operations reported an asynchronous error");
}

void markup_application_event_uses_its_event_time_target() {
    ii::runtime::EventLoop loop; int errors = 0;
    std::vector<std::shared_ptr<Window>> opened;
    auto registry = make_registry(loop, opened, errors);
    auto owner = Markup::make_application_owner();
    good(owner->bind_scene_registry(registry)); good(owner->activate());
    auto scope = owner->scope(); check(bool(scope), "Markup application scope was not activated");
    auto tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "markup_host", {640, 480}));
    good(host.set_content([&](auto& widgets) { return Markup::build_scene_with_application(widgets, {}, owner, 0); }));
    scope->backend_0.target.set("tools"); good(owner->notify_ready());
    check(opened.empty(), "Application event opened a window synchronously");
    scope->backend_0.target.set("palette"); loop.poll();
    check(opened.size() == 1 && opened[0]->name == "tools" && errors == 0,
          "Markup application event did not capture its selected target");
    good(registry->shutdown()); good(host.clear_content()); good(owner->retire()); tree->close();
}
}

int main() { dynamic_targets_follow_event_time_and_scope_lifetime(); markup_application_event_uses_its_event_time_target(); }
