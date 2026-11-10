#include "LVUINativeWindowCloseFixture.generated.h"
#include "LVUINativeSingleWindowCloseFixture.generated.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
using Multi = lvui_generated::LVUINativeWindowCloseFixture;
using Single = lvui_generated::LVUINativeSingleWindowCloseFixture;
using State = ii::gui::detail::WidgetState;
using Backend = WindowCloseFixtureBackend;

void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void good(ii::core::Result<void> result) {
    if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); }
}
template<class T> T take(ii::core::Result<T> result) {
    if (!result) { if (result.error().cause) std::rethrow_exception(result.error().cause); throw std::runtime_error(result.error().message); }
    return std::move(*result);
}
void reset() {
    Backend::created = Backend::destroyed = Backend::opened_count = 0;
    Backend::allow.fill(true); Backend::fail = false; Backend::requests.clear();Backend::during_open={};Backend::during_request={};
    NativeApplicationBackend::events.clear();
    NativeApplicationBackend::allow_close_request = true;
    NativeApplicationBackend::fail_close_request = false;
}

void multiple_windows_preserve_scopes_on_veto() {
    reset();
    ii::runtime::EventLoop loop;
    auto owner = Multi::make_application_owner();
    good(owner->bind_scene_loop(loop)); good(owner->bind_close_loop(loop)); good(owner->activate());
    auto windows = std::make_shared<lvui::native::SceneWindowSet>(2, [owner]() { return owner->request_close(); });
    auto first_guard = std::make_shared<lvui::native::WindowCloseGuard>();
    auto second_guard = std::make_shared<lvui::native::WindowCloseGuard>();
    auto first = take(windows->activate(0, false, first_guard));
    auto second = take(windows->activate(1, false, second_guard));
    auto first_tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto second_tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto first_host = take(first_tree->content_host({}, "first", {640, 400}));
    auto second_host = take(second_tree->content_host({}, "second", {640, 400}));
    good(first_host.set_content([&](auto& widgets) { return Multi::build_scene_with_application(widgets, {}, owner, 0, first_guard); }));
    good(second_host.set_content([&](auto& widgets) { return Multi::build_scene_with_application(widgets, {}, owner, 1, second_guard); }));
    good(owner->notify_ready());
    check(Backend::created == 2 && Backend::opened_count == 2, "Window scopes did not publish independently");

    auto link = std::make_shared<std::weak_ptr<lvui::native::SceneWindowSet>>(windows);
    int accepted = 0;
    auto quit = Multi::all_windows_close_request_handler(owner, link, [&] { ++accepted; });
    Backend::allow[1] = false;
    check(!quit() && Backend::requests == std::vector<int>({0, 1}) && NativeApplicationBackend::events.empty() &&
          Backend::destroyed == 0 && accepted == 0, "A denied window caused partial application shutdown");
    Backend::requests.clear(); Backend::allow[1] = true;
    NativeApplicationBackend::allow_close_request = false;
    check(!quit() && Backend::requests == std::vector<int>({0, 1}) &&
          NativeApplicationBackend::events == std::vector<std::string>{"close-request"} && Backend::destroyed == 0,
          "Application veto did not preserve both published window scopes");
    Backend::requests.clear(); NativeApplicationBackend::events.clear();
    NativeApplicationBackend::allow_close_request = true;
    check(quit() && accepted == 1 && Backend::requests == std::vector<int>({0, 1}) &&
          NativeApplicationBackend::events == std::vector<std::string>{"close-request"},
          "Application quit did not preflight each window and then the app");

    Backend::requests.clear(); NativeApplicationBackend::events.clear();
    Backend::allow[0] = false;
    check(!take(windows->request_close(first)) && take(windows->active_count()) == 2 &&
          NativeApplicationBackend::events.empty(), "Individual window veto asked to terminate the application");
    Backend::allow[0] = true;
    check(take(windows->request_close(first)) && NativeApplicationBackend::events.empty(),
          "Closing one of two windows requested application termination");
    good(first_host.clear_content()); (void)take(windows->closed(first));
    auto reopened_guard = std::make_shared<lvui::native::WindowCloseGuard>();
    auto reopened = take(windows->activate(0, false, reopened_guard));
    auto reopened_tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto reopened_host = take(reopened_tree->content_host({}, "reopened", {640, 400}));
    good(reopened_host.set_content([&](auto& widgets) { return Multi::build_scene_with_application(widgets, {}, owner, 0, reopened_guard); }));
    Backend::allow[2] = false;
    check(Backend::created == 3 && Backend::destroyed == 1 && !take(windows->request_close(reopened)),
          "Reopening a window did not install an independent fresh close policy");
    Backend::allow[2] = true;
    check(take(windows->request_close(reopened)), "The reopened window could not retry its close");
    good(reopened_host.clear_content()); (void)take(windows->closed(reopened));
    reopened_tree->close();
    Backend::requests.clear();
    check(take(windows->request_close(second)) && Backend::requests == std::vector<int>{1} &&
          NativeApplicationBackend::events == std::vector<std::string>{"close-request"},
          "Last-window close did not ask the window before the application");
    good(second_host.clear_content()); (void)take(windows->closed(second));
    good(owner->retire()); first_tree->close(); second_tree->close();
    check(Backend::destroyed == 3, "Window backend scopes were not retired");
}

void single_markup_window_uses_its_policy() {
    reset();
    ii::runtime::EventLoop loop;
    auto owner = Single::make_application_owner();
    good(owner->bind_scene_loop(loop)); good(owner->bind_close_loop(loop)); good(owner->activate());
    auto guard = std::make_shared<lvui::native::WindowCloseGuard>();
    auto tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "single", {640, 400}));
    good(host.set_content([&](auto& widgets) { return Single::build_with_application(widgets, {}, owner, guard); }));
    good(owner->notify_ready());
    int accepted = 0;
    auto close = Single::window_close_request_handler(owner, guard, [&] { ++accepted; });
    bool wrong_thread = true;
    std::thread foreign([&] { wrong_thread = close(); });
    foreign.join();
    check(!wrong_thread && Backend::requests.empty(), "A foreign thread invoked a window close policy");
    Backend::allow[0] = false;
    check(!close() && NativeApplicationBackend::events.empty() && Backend::destroyed == 0,
          "A single window veto did not prevent application approval");
    Backend::allow[0] = true;
    bool nested = true;
    NativeApplicationBackend::during_close_request = [&] { nested = close(); };
    check(close() && accepted == 1 && NativeApplicationBackend::events == std::vector<std::string>{"close-request"},
          "A single markup window did not approve window and application close");
    check(!nested && Backend::requests == std::vector<int>({0, 0}),
          "A nested application close invoked the window policy twice");
    NativeApplicationBackend::during_close_request = {};
    good(host.clear_content());
    good(guard->prepare());
    good(host.set_content([&](auto& widgets) { return Single::build_with_application(widgets, {}, owner, guard); }));
    check(close() && accepted == 2 && Backend::requests == std::vector<int>({0, 0, 0}),
          "Reattaching a view did not install a fresh window close policy");
    good(host.clear_content()); good(owner->retire()); tree->close();
    check(Backend::destroyed == 1, "Single-window backend did not retire");
}

void policy_failure_is_preserved_through_retirement() {
    reset();
    ii::runtime::EventLoop loop;
    auto owner = Single::make_application_owner();
    good(owner->bind_scene_loop(loop)); good(owner->bind_close_loop(loop)); good(owner->activate());
    auto guard = std::make_shared<lvui::native::WindowCloseGuard>();
    auto tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "failure", {640, 400}));
    good(host.set_content([&](auto& widgets) { return Single::build_with_application(widgets, {}, owner, guard); }));
    good(owner->notify_ready());
    auto close = Single::window_close_request_handler(owner, guard, [] {});
    Backend::fail = true;
    check(!close() && loop.state() == ii::runtime::LoopState::stopped && NativeApplicationBackend::events.empty(),
          "A throwing window policy did not stop and reject the application close");
    good(host.clear_content());
    auto outcome = owner->retire();
    check(!outcome && outcome.error().message == "Window close request callback failed" && outcome.error().cause,
          "The window close failure did not survive application retirement");
    tree->close(); Backend::fail = false;
}

void multi_window_policy_failure_stops_the_application() {
    reset();
    ii::runtime::EventLoop loop;
    auto owner = Multi::make_application_owner();
    good(owner->bind_scene_loop(loop)); good(owner->bind_close_loop(loop)); good(owner->activate());
    auto startup = std::make_shared<lvui::native::SceneStartup>(2, []() -> ii::core::Result<void> { return {}; });
    auto windows = std::make_shared<lvui::native::SceneWindowSet>(2, [owner] { return owner->request_close(); });
    auto guard = std::make_shared<lvui::native::WindowCloseGuard>();
    auto first = take(windows->activate(0, false, guard));
    (void)take(windows->activate(1));
    auto tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "failure", {640, 400}));
    good(host.set_content([&](auto& widgets) { return Multi::build_scene_with_application(widgets, {}, owner, 0, guard); }));
    ii::gui::HostCallbacks callbacks;
    Multi::bind_scene_window(callbacks, windows, first, []() -> ii::core::Result<void> { return {}; },
                             Multi::scene_failure_handler(startup, loop));
    Backend::fail = true;
    check(!callbacks.close_requested() && loop.state() == ii::runtime::LoopState::stopped &&
          NativeApplicationBackend::events.empty(), "A multi-window policy exception was accepted");
    auto outcome = startup->outcome();
    check(!outcome && outcome.error().message == "Window close request callback failed" && outcome.error().cause,
          "A multi-window policy failure did not reach scene startup");
    good(host.clear_content()); good(owner->retire()); tree->close(); Backend::fail = false;
}
} // namespace
int main() {
    multiple_windows_preserve_scopes_on_veto();
    single_markup_window_uses_its_policy();
    policy_failure_is_preserved_through_retirement();
    multi_window_policy_failure_stops_the_application();
}
