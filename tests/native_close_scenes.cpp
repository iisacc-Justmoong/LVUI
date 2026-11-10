#include "LVUINativeCloseScenesFixture.generated.h"
#include "../src/Runtime/SceneWindowSet.h"
#include <stdexcept>

namespace {
using View = lvui_generated::LVUINativeCloseScenesFixture;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
template<class T> T take(ii::core::Result<T> value) { if (!value) throw std::runtime_error(value.error().message); return *value; }

void independent_native_window_callbacks() {
    int decisions = 0, emptied = 0, failures = 0, existing_closed = 0;
    bool allow = false;
    auto windows = std::make_shared<lvui::native::SceneWindowSet>(2, [&]() -> ii::core::Result<bool> {
        ++decisions; return allow;
    });
    auto first = take(windows->activate(0)), second = take(windows->activate(1));
    ii::gui::HostCallbacks one, two;
    one.closed = [&] { ++existing_closed; };
    auto empty = [&]() -> ii::core::Result<void> { ++emptied; return {}; };
    auto failure = [&](const ii::core::Error&) { ++failures; };
    View::bind_scene_window(one, windows, first, empty, failure);
    View::bind_scene_window(two, windows, second, empty, failure);
    check(one.close_requested() && decisions == 0 && !two.close_requested() && decisions == 1,
          "Native close callbacks did not preserve another window or veto the last one");
    one.closed();
    allow = true;
    check(two.close_requested() && decisions == 2, "The last window could not retry its close");
    two.closed();
    check(existing_closed == 1 && emptied == 0 && failures == 0, "Expected native close was misreported");

    auto third = take(windows->activate(0));
    ii::gui::HostCallbacks forced;
    View::bind_scene_window(forced, windows, third, empty, failure);
    forced.closed(); forced.closed();
    check(emptied == 1 && failures == 0, "A forced final close did not request termination once");
}

void last_window_termination_is_deferred_and_approved_once() {
    using Backend = NativeApplicationBackend;
    Backend::events.clear(); Backend::allow_close_request = false;
    ii::runtime::EventLoop loop;
    auto owner = View::make_application_owner();
    good(owner->bind_close_loop(loop)); good(owner->activate());
    auto approved = std::make_shared<bool>(false);
    int cancelled = 0, driver_closes = 0;
    auto cancel = [&] { ++cancelled; };
    auto normal_close = View::close_request_handler(owner, cancel);
    auto driver = [&, normal_close]() -> ii::core::Result<void> {
        ++driver_closes;
        const bool accepted = std::exchange(*approved, false) || normal_close();
        check(accepted, "An already approved application close was asked twice");
        return {};
    };
    auto last = View::last_scene_close(owner, loop, approved, cancel, driver);
    check(!take(last()) && cancelled == 0 && driver_closes == 0, "A veto still initiated shutdown");
    loop.poll();
    check(driver_closes == 0, "A veto queued the driver close");
    Backend::allow_close_request = true;
    check(take(last()) && *approved && cancelled == 1 && driver_closes == 0,
          "An approved last close was not deferred or did not stop admission");
    loop.poll();
    check(driver_closes == 1 && !*approved && cancelled == 1 &&
          Backend::events == std::vector<std::string>({"close-request", "close-request"}),
          "Deferred driver close repeated the application decision");
    good(owner->retire());
    Backend::allow_close_request = true;
}

void last_window_failure_preserves_admission_and_error() {
    using Backend = NativeApplicationBackend;
    Backend::events.clear(); Backend::allow_close_request = true;
    ii::runtime::EventLoop stopped_loop;
    auto owner = View::make_application_owner();
    good(owner->bind_close_loop(stopped_loop)); good(owner->activate());
    stopped_loop.request_stop();
    auto approved = std::make_shared<bool>(false);
    int cancelled = 0, calls = 0;
    auto last = View::last_scene_close(owner, stopped_loop, approved, [&] { ++cancelled; },
        [&]() -> ii::core::Result<void> { ++calls; return {}; });
    auto rejected = last();
    check(!rejected && rejected.error().code == ii::core::ErrorCode::closed && !*approved && cancelled == 0,
          "A rejected driver dispatch committed application shutdown");
    good(owner->retire());

    ii::runtime::EventLoop loop;
    owner = View::make_application_owner();
    good(owner->bind_close_loop(loop)); good(owner->activate());
    approved = std::make_shared<bool>(false);
    auto throwing_cancel = View::last_scene_close(owner, loop, approved,
        [] { throw std::runtime_error("cancel failure"); },
        [&]() -> ii::core::Result<void> { ++calls; return {}; });
    auto failed = throwing_cancel();
    check(!failed && failed.error().cause && !*approved, "Cancellation failure was hidden");
    loop.poll();
    check(calls == 0, "A rejected shutdown delivered its queued driver close");
    good(owner->retire());

    ii::runtime::EventLoop failing_loop;
    owner = View::make_application_owner();
    good(owner->bind_close_loop(failing_loop)); good(owner->activate());
    approved = std::make_shared<bool>(false);
    auto failing_driver = View::last_scene_close(owner, failing_loop, approved, [] {},
        []() -> ii::core::Result<void> { return ii::core::fail(ii::core::ErrorCode::failed, "driver close failed"); });
    check(take(failing_driver()), "A valid deferred close was rejected");
    failing_loop.poll();
    auto outcome = owner->retire();
    check(!outcome && outcome.error().message == "driver close failed" && !*approved &&
          failing_loop.state() == ii::runtime::LoopState::stopped,
          "A deferred driver failure did not survive application cleanup");
}

void accepted_last_close_cancels_queued_reopening() {
    using Backend = NativeApplicationBackend;
    Backend::events.clear(); Backend::allow_close_request = true;
    ii::runtime::EventLoop loop;
    int opened = 0, errors = 0, driver_closes = 0;
    auto owner = View::make_application_owner();
    auto registry = std::make_shared<lvui::native::SceneRegistry>(
        [sender = loop.sender()](auto task) -> ii::core::Result<void> {
            if (!sender.post(std::move(task))) return ii::core::fail(ii::core::ErrorCode::closed, "Scene queue closed");
            return {};
        }, [&](const auto&) { ++errors; });
    good(registry->define("tools", [&](bool) -> ii::core::Result<lvui::native::SceneInstance> {
        ++opened;
        auto state = std::make_shared<bool>(false);
        return lvui::native::SceneInstance{state, [state]() -> ii::core::Result<bool> { return *state; },
            [state]() -> ii::core::Result<void> { *state = true; return {}; }};
    }));
    good(owner->bind_scene_registry(registry)); good(owner->bind_close_loop(loop)); good(owner->activate());
    Backend::during_close_request = [&] { good(registry->open("tools")); };
    auto approved = std::make_shared<bool>(false);
    auto last = View::last_scene_close(owner, loop, approved, [&] { good(registry->cancel()); },
        [&]() -> ii::core::Result<void> { ++driver_closes; *approved = false; return {}; });
    check(take(last()), "The last close was not approved");
    Backend::during_close_request = {};
    loop.poll();
    check(opened == 0 && errors == 0 && driver_closes == 1,
          "A scene queued during approval reopened after accepted shutdown");
    good(registry->shutdown()); good(owner->retire());
}
}
int main() {
    independent_native_window_callbacks();
    last_window_termination_is_deferred_and_approved_once();
    last_window_failure_preserves_admission_and_error();
    accepted_last_close_cancels_queued_reopening();
    NativeApplicationBackend::events.clear();
    NativeApplicationBackend::fail_close_request = true;
    ii::runtime::EventLoop loop;
    auto owner = View::make_application_owner();
    auto startup = std::make_shared<lvui::native::SceneStartup>(
        std::vector<bool>(View::initial_scenes.begin(), View::initial_scenes.end()), []() -> ii::core::Result<void> { return {}; });
    good(owner->bind_close_loop(loop));
    good(owner->bind_close_failure_handler(View::scene_failure_handler(startup, loop)));
    good(owner->activate());
    auto close = View::close_request_handler(owner, [] {});
    check(!close() && loop.state() == ii::runtime::LoopState::stopped, "A startup close failure was accepted");
    const auto failure = startup->outcome();
    check(!failure && failure.error().message == "Application close request callback failed" && failure.error().cause,
          "Scene startup did not preserve the application close failure");
    const auto later = startup->closed(0);
    check(!later && later.error().message == failure.error().message,
          "A later scene close replaced the original application error");
    const auto retired = owner->retire();
    check(!retired && retired.error().message == failure.error().message, "Application retirement lost the close failure");
    NativeApplicationBackend::fail_close_request = false;
}
