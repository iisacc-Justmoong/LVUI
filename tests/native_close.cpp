#include "LVUINativeCloseFixture.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using View = lvui_generated::LVUINativeCloseFixture;
using Backend = NativeApplicationBackend;
using Commands = lvui::native::ApplicationCommands;
using State = ii::gui::detail::WidgetState;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return std::move(*value);
}
ii::gui::Widget find(const std::shared_ptr<State>& tree, const std::string& name) {
    for (const auto& node : take(tree->snapshots()))
        if (node.id.ends_with("/" + name)) return take(tree->find(node.id));
    throw std::runtime_error("Generated control was not found");
}
void reset() {
    Backend::events.clear(); Backend::file_events.clear(); Backend::allow_close_request = true;
    Backend::fail_close_request = false; Backend::fail_stop = false;
    Backend::during_close_request = {}; Backend::during_start = {};
}

void veto_and_deferred_retry() {
    reset(); Backend::allow_close_request = false;
    ii::runtime::EventLoop loop;
    auto owner = View::make_application_owner();
    good(owner->bind_close_loop(loop));
    int accepted = 0, close_calls = 0;
    bool nested = false, wrong_thread = false;
    std::shared_ptr<Commands> commands;
    auto close_handler = View::close_request_handler(owner, [&] {
        ++accepted; good(commands->cancel()); good(owner->cancel_file_events());
    });
    auto early = owner->request_close();
    check(early && *early, "A close before application activation was not allowed");
    commands = std::make_shared<Commands>([sender = loop.sender()](auto task) -> ii::core::Result<void> {
        if (!sender.post(std::move(task))) return ii::core::fail(ii::core::ErrorCode::closed, "Event loop rejected command");
        return {};
    }, [&]() -> ii::core::Result<void> { ++close_calls; (void)close_handler(); return {}; },
       [owner](const auto& error) { (void)owner->report_close_failure(error); });
    good(owner->bind_application_commands(commands));
    Backend::during_start = [&] { auto attempt = owner->request_close();
        check(attempt && !*attempt, "A reentrant start callback permitted close"); };
    good(owner->activate()); Backend::during_start = {};
    auto tree = std::make_shared<State>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "root", {640, 400}));
    good(host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }));
    good(owner->notify_ready());
    Backend::during_close_request = [&] {
        auto retry = owner->request_close(); nested = retry && !*retry;
        std::thread foreign([&] { auto value = owner->request_close();
            wrong_thread = !value && value.error().code == ii::core::ErrorCode::wrong_executor; });
        foreign.join();
        good(owner->notify_files({"/after-veto"})); good(owner->notify_activity(true));
        check(Backend::file_events.empty(), "A file event ran inside the close callback");
    };
    auto button = find(tree, "quit_button");
    good(button.activate()); good(button.activate());
    check(close_calls == 0, "Declarative quit closed the host inside the control callback");
    loop.poll();
    check(close_calls == 1 && accepted == 0 && nested && wrong_thread &&
          Backend::events == std::vector<std::string>({"start", "ready", "close-request", "active", "files"}) &&
          Backend::file_events == std::vector<std::vector<std::string>>{{"/after-veto"}},
          "A veto lost the app, deferred events, or thread/reentry guarantees");
    check(take(find(tree, "close_label").snapshot()).text == "/after-veto", "A vetoed callback did not publish its state");
    Backend::during_close_request = [&] {
        good(owner->notify_files({"/discard-on-close"}));
        good(owner->notify_activity(false));
    };
    Backend::allow_close_request = true;
    good(find(tree, "quit_nested").activate()); loop.poll();
    check(close_calls == 2 && accepted == 1 && !commands->quit() && !owner->notify_files({"/late"}),
          "Accepted close did not cancel pending admissions");
    good(host.clear_content()); good(owner->retire()); tree->close();
    check(Backend::events == std::vector<std::string>({"start", "ready", "close-request", "active", "files", "close-request", "stop"}),
          "Application stop was not paired after an approved close");
    reset();
}

void failure_stops_and_preserves_the_original() {
    reset(); Backend::fail_close_request = true; Backend::fail_stop = true;
    ii::runtime::EventLoop loop;
    auto owner = View::make_application_owner();
    good(owner->bind_close_loop(loop)); good(owner->activate());
    int accepted = 0;
    auto close = View::close_request_handler(owner, [&] { ++accepted; });
    check(!close() && accepted == 0 && loop.state() == ii::runtime::LoopState::stopped,
          "A failing close decision was accepted or did not stop the loop");
    auto outcome = owner->retire();
    check(!outcome && outcome.error().cause && outcome.error().message == "Application close request callback failed",
          "A later stop failure replaced the original close error");
    reset();
}
void failure_reaches_application_result() {
    reset(); Backend::fail_close_request = true; Backend::fail_stop = true;
    ii::runtime::Application app({}, {.workers={.workers=1}});
    auto owner = View::make_application_owner();
    good(owner->bind_close_loop(app.event_loop()));
    good(app.attach_before_driver(View::application_attachment("application", owner)));
    auto close = View::close_request_handler(owner, [] {});
    good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> {
        if (auto started = start(); !started) return started;
        check(!close(), "A failed close callback permitted native close");
        return {};
    }, []() -> ii::core::Result<void> { return {}; }}));
    const auto result = app.exec();
    check(!result && result.error().message == "Application close request callback failed" && result.error().cause &&
          app.event_loop().state() == ii::runtime::LoopState::stopped &&
          Backend::events == std::vector<std::string>({"start", "close-request", "stop"}),
          "The application result did not preserve the close callback failure through shutdown");
    reset();
}
void foreign_close_request_is_refused() {
    reset();
    ii::runtime::EventLoop loop;
    auto owner = View::make_application_owner();
    good(owner->bind_close_loop(loop)); good(owner->activate());
    int accepted = 0;
    auto close = View::close_request_handler(owner, [&] { ++accepted; });
    bool refused = false, failure_report_refused = false;
    const auto previous = std::set_terminate([] { std::_Exit(73); });
    std::thread foreign([&] {
        refused = !close();
        auto report = owner->report_close_failure({ii::core::ErrorCode::failed, "foreign failure"});
        failure_report_refused = !report && report.error().code == ii::core::ErrorCode::wrong_executor;
    });
    foreign.join();
    std::set_terminate(previous);
    check(refused && failure_report_refused && accepted == 0 && loop.state() != ii::runtime::LoopState::stopped &&
          Backend::events == std::vector<std::string>({"start"}),
          "A foreign close request changed application state or stopped the loop");
    good(owner->retire()); reset();
}
}
int main() { veto_and_deferred_retry(); failure_stops_and_preserves_the_original(); failure_reaches_application_result(); foreign_close_request_is_refused(); }
