#include <LVUI/SceneStartup.h>
#include <stdexcept>
#include <thread>

namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void good(ii::core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }

void waits_for_every_scene() {
    int ready = 0;
    lvui::native::SceneStartup startup(3, [&]() -> ii::core::Result<void> { ++ready; return {}; });
    good(startup.mounted(2));
    good(startup.mounted(0));
    check(ready == 0 && !startup.ready(), "The app became ready before every scene mounted");
    auto duplicate = startup.mounted(0);
    check(!duplicate && duplicate.error().code == ii::core::ErrorCode::already_exists,
          "A duplicate scene mount was accepted");
    auto invalid = startup.mounted(3);
    check(!invalid && invalid.error().code == ii::core::ErrorCode::out_of_range,
          "An unknown scene was accepted");
    good(startup.mounted(1));
    check(ready == 1 && startup.ready(), "The app did not become ready exactly once");
    good(startup.closed(1));
    good(startup.outcome());
}

void waits_only_for_initial_scenes() {
    int ready = 0;
    lvui::native::SceneStartup startup(std::vector<bool>{true, false, true}, [&]() -> ii::core::Result<void> { ++ready; return {}; });
    check(startup.accepts_runtime(), "A delayed scene was rejected during startup");
    good(startup.closed(1));
    good(startup.mounted(0));
    check(ready == 0 && !startup.ready(), "An optional window changed the initial readiness count");
    auto optional = startup.mounted(1);
    check(!optional && optional.error().code == ii::core::ErrorCode::invalid_argument,
          "A delayed scene was counted as an initial scene");
    good(startup.mounted(2));
    check(ready == 1 && startup.ready() && startup.accepts_runtime(), "Initial scenes did not complete readiness");
}

void failures_and_cancellation() {
    lvui::native::SceneStartup failed(2, []() -> ii::core::Result<void> { return {}; });
    good(failed.mounted(0));
    auto closed = failed.closed(1);
    check(!closed && closed.error().code == ii::core::ErrorCode::closed && failed.failed(),
          "A scene closed during startup without recording failure");
    (void)failed.fail({ii::core::ErrorCode::failed, "later error"});
    auto outcome = failed.outcome();
    check(!outcome && outcome.error().code == ii::core::ErrorCode::closed,
          "A later error replaced the first startup error");
    bool cleaned = false;
    auto failed_cleanup = failed.finish([&]() -> ii::core::Result<void> {
        cleaned = true;
        return ii::core::fail(ii::core::ErrorCode::failed, "cleanup failed");
    });
    check(cleaned && !failed_cleanup && failed_cleanup.error().code == ii::core::ErrorCode::closed,
          "A cleanup failure hid the first startup failure");
    auto throwing_cleanup = failed.finish([]() -> ii::core::Result<void> { throw std::runtime_error("cleanup exception"); });
    check(!throwing_cleanup && throwing_cleanup.error().code == ii::core::ErrorCode::closed,
          "A cleanup exception hid the first startup failure");
    good(failed.cancel());
    check(!failed.outcome() && !failed.accepts_runtime(), "Cancellation erased failure or admitted later scenes");

    int called = 0;
    lvui::native::SceneStartup canceled(2, [&]() -> ii::core::Result<void> { ++called; return {}; });
    good(canceled.mounted(0));
    good(canceled.cancel());
    check(!canceled.accepts_runtime(), "Cancellation admitted later scenes");
    good(canceled.closed(1));
    check(!canceled.mounted(1) && canceled.outcome() && !canceled.failed() && called == 0,
          "A normal shutdown was reported as a startup failure");

    lvui::native::SceneStartup callback(1, []() -> ii::core::Result<void> {
        return ii::core::fail(ii::core::ErrorCode::failed, "ready failed");
    });
    check(!callback.mounted(0) && callback.failed() && !callback.outcome(),
          "Readiness failure was not preserved");
    lvui::native::SceneStartup throws(1, []() -> ii::core::Result<void> { throw std::runtime_error("ready exception"); });
    auto thrown = throws.mounted(0);
    check(!thrown && thrown.error().cause && throws.failed(), "Readiness exception was lost");
    lvui::native::SceneStartup successful(1, []() -> ii::core::Result<void> { return {}; });
    good(successful.mounted(0));
    auto cleanup_error = successful.finish([]() -> ii::core::Result<void> {
        return ii::core::fail(ii::core::ErrorCode::failed, "cleanup failed");
    });
    check(!cleanup_error && cleanup_error.error().message == "cleanup failed", "A cleanup failure was lost");
    auto cleanup_exception = successful.finish([]() -> ii::core::Result<void> { throw std::runtime_error("cleanup exception"); });
    check(!cleanup_exception && cleanup_exception.error().cause, "A cleanup exception was lost");

    bool first_scene_alive = true;
    lvui::native::SceneStartup delayed_close(2, [&]() -> ii::core::Result<void> {
        if (!first_scene_alive) return ii::core::fail(ii::core::ErrorCode::closed, "scene closed before ready");
        return {};
    });
    good(delayed_close.mounted(0));
    first_scene_alive = false; // The native close notification has not been delivered yet.
    check(!delayed_close.mounted(1) && delayed_close.failed() && !delayed_close.outcome(),
          "A scene closed before readiness was accepted while its notification was delayed");

    lvui::native::SceneStartup* current = nullptr;
    lvui::native::SceneStartup cancel_then_fail(1, [&]() -> ii::core::Result<void> {
        good(current->cancel());
        return ii::core::fail(ii::core::ErrorCode::failed, "error during cancellation");
    });
    current = &cancel_then_fail;
    check(!cancel_then_fail.mounted(0) && cancel_then_fail.failed() && !cancel_then_fail.outcome(),
          "Cancellation hid an error from the active readiness callback");
}

void threading_and_reentry() {
    lvui::native::SceneStartup* pointer = nullptr;
    bool rejected = false;
    lvui::native::SceneStartup startup(1, [&]() -> ii::core::Result<void> {
        auto result = pointer->mounted(0);
        rejected = !result && result.error().code == ii::core::ErrorCode::busy;
        return {};
    });
    pointer = &startup;
    bool wrong = false;
    std::thread worker([&] { auto result = startup.mounted(0); wrong = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join();
    check(wrong, "A foreign thread changed scene startup");
    good(startup.mounted(0));
    check(rejected && startup.ready(), "Readiness callback reentry was accepted");
}
} // namespace

int main() { waits_for_every_scene(); waits_only_for_initial_scenes(); failures_and_cancellation(); threading_and_reentry(); }
