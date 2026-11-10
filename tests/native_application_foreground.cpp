#include "LVUINativeForegroundFixture.generated.h"
#include "LVUINativeForegroundMarkupFixture.generated.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using View = lvui_generated::LVUINativeForegroundFixture;
using Markup = lvui_generated::LVUINativeForegroundMarkupFixture;
using Backend = NativeForegroundBackend;

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
void reset() {
    Backend::events.clear();
    Backend::during_foreground = {}; Backend::during_background = {};
    Backend::during_close = {}; Backend::during_files = {};
    Backend::allow_close = true; Backend::fail_background = false;
}
template<class V> auto tree_for(const std::shared_ptr<typename V::ApplicationOwner>& owner) {
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "foreground-root", {640, 400}));
    good(host.set_content([&](auto& widgets) { return V::build_with_application(widgets, {}, owner); }));
    return std::pair{tree, host};
}

void lifecycle_and_reentry() {
    reset();
    auto owner = View::make_application_owner();
    auto early = owner->notify_foreground(true);
    check(!early && early.error().code == ii::core::ErrorCode::closed, "A stopped app accepted foreground state");
    good(owner->activate());
    good(owner->notify_activity(false));
    good(owner->notify_foreground(false));
    good(owner->notify_foreground(true));
    check(Backend::events == std::vector<std::string>{"start"}, "A state escaped before readiness");
    auto [tree, host] = tree_for<View>(owner);
    good(owner->notify_ready());
    check(Backend::events == std::vector<std::string>({"start", "ready", "inactive", "foreground"}),
          "Pre-ready focus and foreground state were not kept separately");
    good(owner->notify_foreground(true));
    check(Backend::events.size() == 4, "An unchanged foreground state was delivered twice");
    good(owner->notify_foreground(false));
    Backend::during_foreground = [&] {
        good(owner->notify_activity(true));
        good(owner->notify_foreground(false));
        good(owner->notify_activity(false));
    };
    good(owner->notify_foreground(true));
    Backend::during_foreground = {};
    check(Backend::events == std::vector<std::string>({"start", "ready", "inactive", "foreground", "background",
                                                      "foreground", "active", "background", "inactive"}),
          "Reentrant foreground and focus transitions were lost or reordered");
    good(owner->notify_foreground(true));
    Backend::during_background = [&] { good(owner->notify_files({"/sample"})); good(owner->notify_activity(true)); };
    good(owner->notify_foreground(false));
    Backend::during_background = {};
    const auto count = Backend::events.size();
    check(count >= 4 && std::vector<std::string>(Backend::events.end()-4, Backend::events.end()) ==
          std::vector<std::string>({"foreground", "background", "active", "files"}),
          "Reentrant files and state events were not drained safely");
    Backend::allow_close = false;
    Backend::during_close = [&] { good(owner->notify_foreground(true)); };
    auto denied = owner->request_close();
    check(denied && !*denied && Backend::events[Backend::events.size()-2] == "close" &&
          Backend::events.back() == "foreground", "A rejected close lost a foreground transition");
    Backend::allow_close = true;
    Backend::during_close = [&] { good(owner->notify_foreground(false)); };
    auto close = View::close_request_handler(owner, [&] { good(owner->cancel_state_events()); });
    check(close() && Backend::events.back() == "close", "An accepted close delivered pending state");
    Backend::during_close = {};
    auto after_accept = owner->notify_foreground(true);
    check(!after_accept && after_accept.error().code == ii::core::ErrorCode::closed && owner->state_events_cancelled(),
          "An accepted close left foreground admission open");
    good(host.clear_content()); good(owner->retire()); tree->close();
    check(Backend::events.back() == "stop", "App retirement did not discard pending state before stop");
    auto late = owner->notify_foreground(true);
    check(!late && late.error().code == ii::core::ErrorCode::closed, "Retired state delivery remained open");
}

void markup_and_failure() {
    reset();
    auto owner = Markup::make_application_owner();
    good(owner->activate());
    auto [tree, host] = tree_for<Markup>(owner);
    good(owner->notify_ready());
    check(Backend::events == std::vector<std::string>({"start", "ready"}), "Unknown foreground state was fabricated");
    bool wrong_thread = false;
    std::thread worker([&] {
        auto result = owner->notify_foreground(true);
        wrong_thread = !result && result.error().code == ii::core::ErrorCode::wrong_executor;
    });
    worker.join();
    check(wrong_thread, "Foreground state accepted a foreign thread");
    Backend::fail_background = true;
    auto failed = owner->notify_foreground(false);
    check(!failed && failed.error().cause, "A background callback exception was lost");
    try { std::rethrow_exception(failed.error().cause); }
    catch (const std::runtime_error& error) { check(std::string(error.what()) == "background callback failure", "The callback cause was replaced"); }
    Backend::fail_background = false;
    auto closed = owner->notify_foreground(true);
    check(!closed && closed.error().code == ii::core::ErrorCode::closed, "State delivery continued after failure");
    good(host.clear_content()); good(owner->retire()); tree->close();
}

void bounded_reentry() {
    reset();
    auto owner = View::make_application_owner(); good(owner->activate());
    auto [tree, host] = tree_for<View>(owner); good(owner->notify_ready());
    Backend::during_foreground = [&] { good(owner->notify_foreground(false)); };
    Backend::during_background = [&] { good(owner->notify_foreground(true)); };
    auto limited = owner->notify_foreground(true);
    Backend::during_foreground = {}; Backend::during_background = {};
    check(!limited && limited.error().code == ii::core::ErrorCode::full, "Foreground reentry was unbounded");
    good(host.clear_content()); good(owner->retire()); tree->close();
}

void callback_failure_reaches_application() {
    reset(); Backend::fail_background = true;
    auto owner = View::make_application_owner();
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "failure-root", {640, 400}));
    ii::runtime::Application app({}, {.workers = {.workers = 1}});
    good(app.attach_before_driver(View::application_attachment("application", owner)));
    good(app.attach({"window", [&](auto&) -> ii::core::Result<void> {
        if (auto mounted = host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }); !mounted) return mounted;
        return owner->notify_ready();
    }, [&]() -> ii::core::Result<void> { return host.clear_content(); }}));
    good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> {
        if (auto started = start(); !started) return started;
        return owner->notify_foreground(false);
    }, [] { return ii::core::Result<void>{}; }}));
    auto result = app.exec(); Backend::fail_background = false;
    check(!result && result.error().cause && Backend::events ==
          std::vector<std::string>({"start", "ready", "background", "stop"}),
          "Foreground failure did not reach Core after window and app cleanup");
    try { std::rethrow_exception(result.error().cause); }
    catch (const std::runtime_error& error) { check(std::string(error.what()) == "background callback failure", "Core replaced the foreground failure cause"); }
    tree->close();
}

void unsupported_is_atomic() {
    if (ii::gui::supports_application_foreground()) return;
    ii::runtime::Application app;
    auto result = View::configure_application(app);
    check(!result && result.error().code == ii::core::ErrorCode::unsupported,
          "A host without foreground events silently accepted the declaration");
    good(app.set_driver({[](const auto& start) { return start(); }, [] { return ii::core::Result<void>{}; }}));
    check(bool(app.exec()), "Unsupported foreground registration left partial application configuration");
}

void desktop_configuration_accepts_foreground() {
    const auto backend=ii::gui::host_capabilities().backend;
    if (backend != ii::gui::HostBackend::cocoa && backend != ii::gui::HostBackend::win32 && backend != ii::gui::HostBackend::x11) return;
    check(ii::gui::supports_application_foreground(), "Desktop host did not advertise foreground support");
    ii::runtime::Application app;
    check(bool(View::configure_application(app)), "Desktop host rejected a generated foreground application");
}
}

int main() {
    lifecycle_and_reentry(); markup_and_failure(); bounded_reentry(); callback_failure_reaches_application(); unsupported_is_atomic(); desktop_configuration_accepts_foreground();
}
