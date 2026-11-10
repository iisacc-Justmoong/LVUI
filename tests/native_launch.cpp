#include "LVUINativeLaunchFixture.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using View = lvui_generated::LVUINativeLaunchFixture;
using Backend = NativeApplicationBackend;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void good(ii::core::Result<void> value) {
    if (!value) throw std::runtime_error(value.error().message);
}
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void reset() {
    Backend::events.clear(); Backend::launch_events.clear(); Backend::file_events.clear();
    Backend::during_launch = {}; Backend::fail_launch = false; Backend::fail_stop = false;
}
void lifecycle() {
    reset();
    const std::vector<std::string> args{"program", "한글 🎨", "", "--flag", "한글 🎨", std::string(1, char(0xff))};
    auto owner = View::make_application_owner();
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "launch", {640, 400}));
    bool reentry = false, premature_ready = false, wrong_thread = false;
    Backend::during_launch = [&] {
        const auto duplicate = owner->notify_launch_arguments(args);
        reentry = !duplicate && duplicate.error().code == ii::core::ErrorCode::busy;
        const auto ready = owner->notify_ready();
        premature_ready = !ready && ready.error().code == ii::core::ErrorCode::busy;
        std::thread foreign([&] {
            const auto value = owner->notify_launch_arguments(args);
            wrong_thread = !value && value.error().code == ii::core::ErrorCode::wrong_executor;
        });
        foreign.join();
        good(owner->notify_files({"file-from-callback"}));
        good(owner->notify_activity(true));
        check(Backend::events == std::vector<std::string>({"start", "arguments"}),
              "File or activity callback ran during launch argument delivery");
    };
    ii::runtime::Application app(args, {.workers={.workers=1}});
    good(app.attach_before_driver(View::application_attachment("application", owner)));
    good(app.attach_before_driver({"pre-launch-probe", [&](auto&) -> ii::core::Result<void> {
        auto outcome = owner->notify_ready();
        check(!outcome && outcome.error().code == ii::core::ErrorCode::closed &&
              outcome.error().message.find("launch arguments") != std::string::npos,
              "Readiness was allowed before launch arguments were delivered");
        return {};
    }, []() -> ii::core::Result<void> { return {}; }}));
    good(app.attach_before_driver(View::launch_arguments_attachment("launch", owner)));
    good(app.attach({"window", [&](auto&) -> ii::core::Result<void> {
        if (auto opened = host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }); !opened) return opened;
        const auto nodes = take(tree->snapshots());
        check(std::ranges::any_of(nodes, [](const auto& node) { return node.id.ends_with("launch_label") && node.text == "한글 🎨"; }),
              "Launch callback state was not published to the first window");
        return owner->notify_ready();
    }, [&]() -> ii::core::Result<void> { return host.clear_content(); }}));
    good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> {
        check(Backend::events == std::vector<std::string>({"start", "arguments"}), "Launch callback did not precede the driver");
        const auto duplicate = owner->notify_launch_arguments(args);
        check(!duplicate && duplicate.error().code == ii::core::ErrorCode::busy, "Launch callback was delivered twice");
        return start();
    }, []() -> ii::core::Result<void> { return {}; }}));
    check(bool(app.exec()), "Launch lifecycle failed");
    check(Backend::launch_events == std::vector<std::vector<std::string>>{args}, "Arguments were modified or reordered");
    check(Backend::events == std::vector<std::string>({"start", "arguments", "ready", "active", "files", "stop"}),
          "Launch, ready, queued events and shutdown occurred out of order");
    check(Backend::file_events == std::vector<std::vector<std::string>>{{"file-from-callback"}} && reentry && premature_ready && wrong_thread,
          "Launch callbacks violated reentry, thread or queue contracts");
    Backend::during_launch = {}; tree->close();
}
void failure() {
    reset(); Backend::fail_launch = true; Backend::fail_stop = true;
    auto owner = View::make_application_owner();
    ii::runtime::Application app({"program"}, {.workers={.workers=1}});
    good(app.attach_before_driver(View::application_attachment("application", owner)));
    good(app.attach_before_driver(View::launch_arguments_attachment("launch", owner)));
    bool driver_started = false;
    good(app.set_driver({[&](const auto&) -> ii::core::Result<void> { driver_started = true; return {}; },
                         []() -> ii::core::Result<void> { return {}; }}));
    auto outcome = app.exec();
    check(!outcome && outcome.error().code == ii::core::ErrorCode::failed && outcome.error().cause && !driver_started,
          "A failed launch callback did not stop the native driver with its error");
    bool original_failure = false;
    try { std::rethrow_exception(outcome.error().cause); }
    catch (const std::runtime_error& error) { original_failure = std::string(error.what()) == "launch callback failure"; }
    check(original_failure, "A later shutdown failure replaced the launch callback failure");
    check(Backend::events == std::vector<std::string>({"start", "arguments", "stop"}) && !owner->scope(),
          "Failed launch did not retire the started application");
    reset();
}
void empty_arguments() {
    reset();
    auto owner = View::make_application_owner();
    ii::runtime::Application app({}, {.workers={.workers=1}});
    good(app.attach_before_driver(View::application_attachment("application", owner)));
    good(app.attach_before_driver(View::launch_arguments_attachment("launch", owner)));
    good(app.set_driver({[](const auto& start) { return start(); }, []() -> ii::core::Result<void> { return {}; }}));
    check(bool(app.exec()) && Backend::launch_events == std::vector<std::vector<std::string>>{{}} &&
          Backend::events == std::vector<std::string>({"start", "arguments", "stop"}),
          "An empty argument list did not receive one launch event");
}
void configuration_rollback() {
    reset();
    const auto initial_created = Backend::created;
    int retained = 0;
    ii::runtime::Application app({"program"}, {.workers={.workers=1}});
    good(app.attach_before_driver({"launch-arguments", [&](auto&) -> ii::core::Result<void> { ++retained; return {}; },
                                   []() -> ii::core::Result<void> { return {}; }}));
    auto outcome = View::configure_application(app);
    check(!outcome && outcome.error().code == ii::core::ErrorCode::already_exists,
          "A duplicate launch attachment did not roll back generated configuration");
    good(app.set_driver({[](const auto& start) { return start(); }, []() -> ii::core::Result<void> { return {}; }}));
    check(bool(app.exec()) && retained == 1 && Backend::created == initial_created,
          "Rollback removed an existing attachment or activated the generated backend");
}
}
int main() {
    try { lifecycle(); failure(); empty_arguments(); configuration_rollback(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
