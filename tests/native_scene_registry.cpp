#include <LVUI/SceneRegistry.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using ii::core::Error;
using ii::core::ErrorCode;
using ii::core::Result;
using Registry = lvui::native::SceneRegistry;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }
struct Window final { int generation; bool closed = false, veto = false; };

void queued_reopening() {
    std::vector<std::function<void()>> queue;
    std::vector<Error> failures;
    auto registry = std::make_shared<Registry>(
        [&](auto work) -> Result<void> { queue.push_back(std::move(work)); return {}; },
        [&](const Error& error) { failures.push_back(error); });
    int generations = 0; bool first_is_initial = false;
    std::weak_ptr<Window> previous;
    std::shared_ptr<Window> current;
    good(registry->define("tools", [&](bool initial) -> Result<lvui::native::SceneInstance> {
        if (!generations) first_is_initial = initial;
        current = std::make_shared<Window>(++generations);
        previous = current;
        return lvui::native::SceneInstance{current,
            [window = current]() -> Result<bool> { return window->closed; },
            [window = current]() -> Result<void> { if (!window->veto) window->closed = true; return {}; }};
    }));
    check(!registry->define("tools", [](bool) -> Result<lvui::native::SceneInstance> { return {}; }), "Duplicate scene id accepted");
    good(registry->open_initial("tools"));
    check(generations == 1 && first_is_initial, "Initial scene did not use its startup factory");
    std::weak_ptr<Window> first_generation = current;
    good(registry->open("tools")); good(registry->close("tools")); good(registry->open("tools"));
    check(generations == 1 && queue.size() == 3 && !current->closed, "A scene request ran inline");
    for (auto& task : queue) task(); queue.clear();
    check(generations == 2 && current->generation == 2 && !current->closed && first_generation.expired() && failures.empty(),
          "Ordered close and reopen did not replace exactly one window generation");
    current->closed = true; current.reset();
    good(registry->open("tools")); queue.back()(); queue.clear();
    check(generations == 3 && !previous.expired(), "Externally closed window was not reopened");
    current->veto = true;
    good(registry->close("tools")); queue.back()(); queue.clear();
    good(registry->open("tools")); queue.back()(); queue.clear();
    check(generations == 3 && !current->closed, "A vetoed close created a duplicate window");
    good(registry->open("tools"));
    const auto vetoed_shutdown = registry->shutdown();
    check(!vetoed_shutdown && vetoed_shutdown.error().code == ErrorCode::busy && !current->closed,
          "A vetoed shutdown discarded a live scene");
    queue.back()(); queue.clear();
    check(generations == 3, "A queued request survived shutdown");
    current->veto = false;
    good(registry->shutdown());
    check(current->closed && !registry->open("tools"), "Shutdown did not close the scene or reject new requests");
}

void failures_and_retirement() {
    std::vector<std::function<void()>> queue; std::vector<Error> errors;
    bool reject = false;
    auto registry = std::make_shared<Registry>(
        [&](auto work) -> Result<void> { if (reject) return ii::core::fail(ErrorCode::full, "queue full"); queue.push_back(std::move(work)); return {}; },
        [&](const Error& error) { errors.push_back(error); });
    int attempts = 0;
    good(registry->define("bad", [&](bool) -> Result<lvui::native::SceneInstance> {
        ++attempts; return ii::core::fail(ErrorCode::invalid_argument, "factory rejected");
    }));
    check(!registry->open("missing"), "Unknown scene id was accepted");
    reject = true; auto full = registry->open("bad"); reject = false;
    check(!full && full.error().code == ErrorCode::full && queue.empty(), "Queue admission failure was hidden");
    good(registry->open("bad")); good(registry->open("bad"));
    for (auto& task : queue) task(); queue.clear();
    check(attempts == 1 && errors.size() == 1 && errors[0].message == "factory rejected" && !registry->open("bad"),
          "An asynchronous failure was lost or later requests were executed");
    good(registry->shutdown());

    auto inline_registry = std::make_shared<Registry>([](auto work) -> Result<void> { work(); return {}; }, [](const Error&) {});
    good(inline_registry->define("x", [](bool) -> Result<lvui::native::SceneInstance> { return {}; }));
    check(!inline_registry->open("x"), "An inline dispatcher was accepted");
    good(inline_registry->shutdown());

    std::vector<std::function<void()>> rejected_work;
    int rejected_attempts = 0;
    auto partial_dispatch = std::make_shared<Registry>([&](auto work) -> Result<void> {
        rejected_work.push_back(std::move(work)); return ii::core::fail(ErrorCode::full, "not admitted");
    }, [](const Error&) {});
    good(partial_dispatch->define("x", [&](bool) -> Result<lvui::native::SceneInstance> { ++rejected_attempts; return {}; }));
    check(!partial_dispatch->open("x"), "Rejected dispatch reported success");
    rejected_work.front()();
    check(rejected_attempts == 0, "A rejected scene request ran later");
    good(partial_dispatch->shutdown());

    bool wrong_thread = false;
    std::thread worker([&] { auto result = registry->close("bad"); wrong_thread = !result && result.error().code == ErrorCode::wrong_executor; });
    worker.join(); check(wrong_thread, "Another thread controlled application windows");
}

void cancel_does_not_close_inside_the_native_callback() {
    std::vector<std::function<void()>> queue;
    auto registry = std::make_shared<Registry>(
        [&](auto work) -> Result<void> { queue.push_back(std::move(work)); return {}; }, [](const Error&) {});
    auto window = std::make_shared<Window>(1);
    int attempts = 0;
    good(registry->define("window", [&](bool) -> Result<lvui::native::SceneInstance> {
        ++attempts;
        return lvui::native::SceneInstance{window,
            [window]() -> Result<bool> { return window->closed; },
            [window]() -> Result<void> { window->closed = true; return {}; }};
    }));
    good(registry->open_initial("window"));
    good(registry->open("window"));
    good(registry->cancel());
    check(!window->closed && !registry->open("window"), "Cancellation closed a native window inline or accepted new requests");
    queue.front()();
    check(attempts == 1 && !window->closed, "A queued request ran after cancellation");
    good(registry->shutdown());
    check(window->closed, "Shutdown did not finish cancellation cleanup");
}
}
int main() { queued_reopening(); failures_and_retirement(); cancel_does_not_close_inside_the_native_callback(); }
