#include "LVUINativeNamedWindowsFixture.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
using View = lvui_generated::LVUINativeNamedWindowsFixture;
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
    throw std::runtime_error("Generated control was not found");
}
struct Generation final {
    std::shared_ptr<State> tree;
    ii::gui::Widget host;
    bool closed = false;
};

void generated_commands_and_generations() {
    static_assert(View::scene_count == 3 && View::initial_scenes[0] && !View::initial_scenes[1] && View::initial_scenes[2]);
    const int windows_before = NativeWindowBackend::created;
    const int retired_before = NativeWindowBackend::destroyed;
    const int apps_before = NativeApplicationBackend::created;
    NativeApplicationBackend::events.clear();
    NativeApplicationBackend::file_events.clear();
    NativeWindowBackend::scene_events.clear();
    ii::runtime::EventLoop loop;
    int errors = 0, generations = 0;
    auto owner = View::make_application_owner();
    auto registry = std::make_shared<lvui::native::SceneRegistry>(
        [sender = loop.sender()](auto task) -> ii::core::Result<void> {
            if (!sender.post(std::move(task))) return ii::core::fail(ii::core::ErrorCode::closed, "Event loop rejected request");
            return {};
        }, [&](const auto&) { ++errors; });
    std::weak_ptr<Generation> latest;
    good(registry->define("tools", [&](bool initial) -> ii::core::Result<lvui::native::SceneInstance> {
        check(!initial, "The optional window was opened during initial startup");
        auto generation = std::make_shared<Generation>();
        generation->tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
        generation->host = take(generation->tree->content_host({}, "tool_host", {480, 360}));
        good(generation->host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 1); }));
        latest = generation;
        ++generations;
        return lvui::native::SceneInstance{generation,
            [generation]() -> ii::core::Result<bool> { return generation->closed; },
            [generation]() -> ii::core::Result<void> {
                if (auto cleared = generation->host.clear_content(); !cleared) return cleared;
                generation->closed = true;
                generation->tree->close();
                return {};
            }};
    }));
    good(owner->bind_scene_registry(registry));
    good(owner->activate());
    good(owner->notify_files({"/scene-request"}));
    check(NativeApplicationBackend::file_events.empty(), "A named scene delivered files before app readiness");
    auto main = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto main_host = take(main->content_host({}, "main_host", {640, 400}));
    good(main_host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 0); }));
    good(owner->notify_ready());
    check(NativeApplicationBackend::created == apps_before + 1 && NativeWindowBackend::created == windows_before,
          "A closed optional scene activated a window backend");
    auto open = find(main, "open");
    auto close = find(main, "close");
    good(open.activate());
    check(generations == 0, "Opening a window ran inside the button callback");
    loop.poll();
    auto first = latest.lock();
    check(first && generations == 1 && NativeWindowBackend::created == windows_before + 1 &&
          take(find(first->tree, "shared").snapshot()).text == "/scene-request", "Opening lost the shared application file state");
    auto old_button = find(first->tree, "close_self");
    good(old_button.activate());
    check(!first->closed, "Closing a window ran inside its own callback");
    loop.poll();
    check(first->closed && !old_button.snapshot() && NativeWindowBackend::destroyed == retired_before + 1,
          "Closing did not retire the window scope and control");
    good(open.activate()); good(open.activate());
    loop.poll();
    auto second = latest.lock();
    check(second && second != first && generations == 2 && NativeWindowBackend::created == windows_before + 2,
          "Reopening did not create exactly one fresh window scope");
    good(close.activate()); good(open.activate());
    loop.poll();
    auto third = latest.lock();
    check(second->closed && third && third != second && generations == 3,
          "Queued close and reopen did not preserve their order");
    check(NativeApplicationBackend::created == apps_before + 1 && errors == 0,
          "Window replacement recreated the application scope or reported an error");
    good(registry->shutdown());
    check(third->closed && NativeWindowBackend::destroyed == retired_before + 3,
          "Application shutdown did not retire its last window");
    good(main_host.clear_content());
    good(owner->retire());
    main->close();
    check(NativeApplicationBackend::events == std::vector<std::string>({"start", "ready", "files", "stop"}) &&
          NativeWindowBackend::scene_events.size() == 6, "Window and application lifecycle events were not paired");
}

void configuration_is_transactional() {
    if (!ii::gui::host_capabilities().secondary_windows) return;
    ii::runtime::Application app({}, {.workers={.workers=1}});
    good(View::configure_application(app));
    auto duplicate = View::configure_application(app);
    check(!duplicate && duplicate.error().code == ii::core::ErrorCode::already_exists,
          "Generated named scenes accepted a second application configuration");
}

void delayed_host_callbacks_follow_application_lifetime() {
    ii::runtime::EventLoop loop;
    int ready = 0, attached = 0;
    auto startup = std::make_shared<lvui::native::SceneStartup>(std::vector<bool>{true, false}, [&]() -> ii::core::Result<void> {
        ++ready; return {};
    });
    auto host = std::make_shared<ii::gui::Host>(ii::gui::HostOptions{});
    auto callbacks = View::scene_callbacks(host, startup, loop, 1, [&](auto&) -> ii::core::Result<void> { ++attached; return {}; }, false);
    callbacks.opened();
    callbacks.closed();
    check(attached == 1 && ready == 0 && !startup->failed(), "A delayed scene affected initial readiness");
    good(startup->mounted(0));
    callbacks.opened();
    callbacks.closed();
    check(attached == 2 && ready == 1 && startup->ready(), "A reopened scene was not attached after readiness");
    good(startup->cancel());
    callbacks.opened();
    check(attached == 2, "A scene was attached after application cancellation");
}

void first_window_can_close_and_reopen_while_another_remains() {
    struct WindowGeneration final {
        std::shared_ptr<State> tree;
        ii::gui::Widget root;
        ii::gui::HostCallbacks callbacks;
        bool closed = false;
    };
    ii::runtime::EventLoop loop;
    int failures = 0, last_decisions = 0, first_generations = 0;
    auto owner = View::make_application_owner();
    auto windows = std::make_shared<lvui::native::SceneWindowSet>(View::scene_count, [&]() -> ii::core::Result<bool> {
        ++last_decisions; return true;
    });
    auto registry = std::make_shared<lvui::native::SceneRegistry>(
        [sender = loop.sender()](auto task) -> ii::core::Result<void> {
            if (!sender.post(std::move(task))) return ii::core::fail(ii::core::ErrorCode::closed, "Scene queue rejected request");
            return {};
        }, [&](const auto&) { ++failures; });
    std::weak_ptr<WindowGeneration> latest_main, latest_tools;
    auto factory = [&](std::size_t index, std::weak_ptr<WindowGeneration>& latest) {
        return [&, index, latest = &latest](bool) -> ii::core::Result<lvui::native::SceneInstance> {
            auto ticket = windows->activate(index, true);
            if (!ticket) return std::unexpected(ticket.error());
            auto generation = std::make_shared<WindowGeneration>();
            generation->tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
            generation->root = take(generation->tree->content_host({}, "independent", {640, 400}));
            good(generation->root.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, index); }));
            View::bind_scene_window(generation->callbacks, windows, *ticket,
                []() -> ii::core::Result<void> { return {}; }, [&](const auto&) { ++failures; });
            *latest = generation;
            if (index == 0) ++first_generations;
            return lvui::native::SceneInstance{generation,
                [generation]() -> ii::core::Result<bool> { return generation->closed; },
                [generation, &loop]() -> ii::core::Result<void> {
                    if (!generation->callbacks.close_requested()) return {};
                    if (auto result = generation->root.clear_content(); !result) return result;
                    generation->closed = true;
                    if (!loop.sender().post([generation] { generation->callbacks.closed(); generation->tree->close(); }))
                        return ii::core::fail(ii::core::ErrorCode::closed, "Close notification was rejected");
                    return {};
                }};
        };
    };
    good(registry->define("main", factory(0, latest_main)));
    good(registry->define("tools", factory(1, latest_tools)));
    good(owner->bind_scene_registry(registry)); good(owner->activate());
    good(registry->open_initial("main")); good(registry->open_initial("tools"));
    good(owner->notify_ready());
    auto first = latest_main.lock(), tools = latest_tools.lock();
    check(first && tools && take(windows->active_count()) == 2, "Independent test windows did not open");
    good(registry->close("main"));
    good(find(tools->tree, "open_main").activate());
    loop.poll();
    auto reopened = latest_main.lock();
    check(first->closed && reopened && reopened != first && first_generations == 2 &&
          take(windows->active_count()) == 2 && last_decisions == 0 && failures == 0 &&
          take(find(tools->tree, "shared").snapshot()).text == "Ready app state",
          "Closing and reopening the first window damaged its sibling or application scope");
    good(reopened->root.clear_content());
    reopened->closed = true; // Native destruction arrived without close_requested.
    good(find(tools->tree, "open_main").activate());
    check(bool(loop.sender().post([reopened] { reopened->callbacks.closed(); reopened->tree->close(); })),
          "Could not defer the forced-close notification");
    loop.poll();
    auto after_forced_close = latest_main.lock();
    check(after_forced_close && after_forced_close != reopened && first_generations == 3 &&
          take(windows->active_count()) == 2 && last_decisions == 0 && failures == 0,
          "A late forced-close notification damaged a reopened first window");
    good(registry->shutdown());
    loop.poll();
    good(owner->retire());
}
} // namespace
int main() { configuration_is_transactional(); generated_commands_and_generations(); delayed_host_callbacks_follow_application_lifetime(); first_window_can_close_and_reopen_while_another_remains(); }
