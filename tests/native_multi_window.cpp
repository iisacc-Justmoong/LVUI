#include "LVUINativeMultiWindowFixture.generated.h"
#include <LVUI/SceneStartup.h>
#include <iiGUI/HostFailure.h>
#include <iiGUI/UIKitWindowGeometry.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
using View = lvui_generated::LVUINativeMultiWindowFixture;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void good(ii::core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }
template<class T> T take(ii::core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}

bool has_text(const std::shared_ptr<ii::gui::detail::WidgetState>& tree, std::string_view suffix, std::string_view value) {
    auto nodes = take(tree->snapshots());
    return std::ranges::any_of(nodes, [&](const auto& node) { return node.id.ends_with(suffix) && node.text == value; });
}

void scenes_share_only_application_state() {
    const int apps = NativeApplicationBackend::created, windows = NativeWindowBackend::created;
    const int retired_apps = NativeApplicationBackend::destroyed, retired_windows = NativeWindowBackend::destroyed;
    NativeApplicationBackend::events.clear();
    auto owner = View::make_application_owner();
    good(owner->activate());
    auto first = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto second = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto first_host = take(first->content_host({}, "first", {640, 400}));
    auto second_host = take(second->content_host({}, "second", {480, 360}));
    auto invalid = first_host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 2); });
    check(!invalid && invalid.error().code == ii::core::ErrorCode::out_of_range && NativeWindowBackend::created == windows,
          "An unknown scene activated a window scope");
    good(first_host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 0); }));
    good(second_host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 1); }));
    check(NativeApplicationBackend::created == apps + 1 && NativeWindowBackend::created == windows + 2,
          "Window scenes did not independently borrow the app scope");
    lvui::native::SceneStartup startup(View::scene_count, [owner] { return owner->notify_ready(); });
    good(startup.mounted(1));
    check(NativeApplicationBackend::events == std::vector<std::string>{"start"}, "Ready ran before all windows mounted");
    good(startup.mounted(0));
    check(NativeApplicationBackend::events == std::vector<std::string>({"start", "ready"}) &&
          has_text(first, "shared", "Ready app state") && has_text(second, "shared", "Ready app state"),
          "Ready did not update both windows through the shared app scope");
    auto scope = owner->scope();
    scope->backend_0.title = "Shared change";
    scope->publish_native_changes();
    check(has_text(first, "shared", "Shared change") && has_text(second, "shared", "Shared change"),
          "Application updates failed to reach both window scopes");
    good(first_host.clear_content());
    check(NativeWindowBackend::destroyed == retired_windows + 1 && NativeApplicationBackend::destroyed == retired_apps &&
          has_text(second, "shared", "Shared change"), "Closing one window damaged another window or app scope");
    auto premature = owner->retire();
    check(!premature && premature.error().code == ii::core::ErrorCode::busy,
          "The app scope retired while another window was open");
    good(second_host.clear_content());
    scope.reset();
    good(owner->retire());
    check(NativeWindowBackend::destroyed == retired_windows + 2 && NativeApplicationBackend::destroyed == retired_apps + 1 &&
          NativeApplicationBackend::events == std::vector<std::string>({"start", "ready", "stop"}),
          "Window and app scopes did not retire in order");
    first->close(); second->close();
}

void independent_options() {
    auto first = take(View::options_for_scene(0));
    auto second = take(View::options_for_scene(1));
    check(first.title == "Canvas" && first.size.width == 640 && second.title == "LVUI" && second.size.width == 480,
          "A scene lost its own window options");
    check(!View::options_for_scene(2), "Unknown scene options were accepted");
    auto red = take(View::appearance_for_scene(0));
    auto default_accent = take(View::appearance_for_scene(1));
    check(red.primary.red != default_accent.primary.red || red.primary.blue != default_accent.primary.blue,
          "A scene inherited another scene's accent");
}

void dynamic_scene_properties() {
    auto owner = View::make_application_owner();
    good(owner->activate());
    std::string title;
    auto geometry_policy = std::make_shared<ii::gui::detail::UIKitWindowGeometry>(true);
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.window_geometry = [&](const ii::gui::WindowGeometry& requested) -> ii::core::Result<void> {
        auto geometry = geometry_policy->prepare(requested);
        if (geometry.width || geometry.height || geometry.minimum_width || geometry.minimum_height)
            return ii::core::fail(ii::core::ErrorCode::unsupported, "UIKit scene geometry belongs to the operating system");
        if (geometry.title) title = *geometry.title;
        return {};
    };
    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); },
        take(View::appearance_for_scene(1)), std::move(hooks));
    good(state->construct([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 1); }));
    good(state->commit_window_properties());
    geometry_policy->finish_initial();
    check(title == "Native" && take(state->appearance()).font_family == "Native",
          "A dynamic scene did not publish its own initial window properties");
    const auto initial_accent = take(state->appearance()).primary;
    good(owner->notify_ready());
    check(title == "Ready app state" && take(state->appearance()).font_family == "Ready app state",
          "A dynamic scene did not update its title and appearance from app state");
    auto scope = owner->scope();
    scope->backend_0.accent = "purple";
    scope->publish_native_changes();
    const auto changed_accent = take(state->appearance()).primary;
    check(initial_accent.red != changed_accent.red || initial_accent.blue != changed_accent.blue,
          "A scene did not update its own dynamic accent");
    scope.reset();
    state->close();
    good(owner->retire());
}

void scene_events_follow_publication() {
    using Backend = NativeWindowBackend;
    Backend::scene_events.clear();
    const int first_id = Backend::created + 1;
    const int destroyed = Backend::destroyed;
    auto owner = View::make_application_owner();
    good(owner->activate());
    auto first = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto second = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto first_host = take(first->content_host({}, "first_events", {640, 400}));
    auto second_host = take(second->content_host({}, "second_events", {480, 360}));
    good(first_host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 0); }));
    good(second_host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 1); }));
    check(Backend::scene_events == std::vector<std::string>({"open:" + std::to_string(first_id), "open:" + std::to_string(first_id + 1)}),
          "A committed window did not deliver its own opened event");
    good(owner->notify_ready());
    Backend::during_close = [&] { check(Backend::destroyed == destroyed, "The window backend retired before its close event"); };
    good(second_host.clear_content());
    Backend::during_close = {};
    good(first_host.clear_content());
    check(Backend::scene_events == std::vector<std::string>({"open:" + std::to_string(first_id), "open:" + std::to_string(first_id + 1),
              "close:" + std::to_string(first_id + 1), "close:" + std::to_string(first_id)}),
          "Window closed events did not follow their published generations");
    check(Backend::destroyed == destroyed + 2, "Window scopes did not retire after the events");
    good(owner->retire());
    first->close(); second->close();

    Backend::scene_events.clear();
    auto canceled_owner = View::make_application_owner();
    good(canceled_owner->activate());
    auto canceled = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto canceled_host = take(canceled->content_host({}, "canceled_event", {640, 400}));
    auto failed = canceled_host.set_content([&](auto& widgets) -> ii::core::Result<void> {
        if (auto built = View::build_scene_with_application(widgets, {}, canceled_owner, 0); !built) return built;
        return ii::core::fail(ii::core::ErrorCode::failed, "cancel before publication");
    });
    check(!failed && Backend::scene_events.empty(), "An unpublished window delivered a lifecycle event");
    good(canceled_owner->retire()); canceled->close();
}

void scene_event_failure_reaches_application_result() {
    using Backend = NativeWindowBackend;
    Backend::scene_events.clear(); Backend::fail_close = true;
    auto owner = View::make_application_owner();
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "failing_event", {640, 400}));
    ii::runtime::Application app({}, {.workers={.workers=1}});
    good(owner->bind_scene_loop(app.event_loop()));
    good(app.attach_before_driver(View::application_attachment("application", owner)));
    good(app.attach({"window", [&](auto&) -> ii::core::Result<void> {
        if (auto mounted = host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 0); }); !mounted) return mounted;
        return owner->scene_outcome();
    }, [&]() -> ii::core::Result<void> {
        if (auto cleared = host.clear_content(); !cleared) return cleared;
        return owner->scene_outcome();
    }}));
    good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> {
        if (auto started = start(); !started) return started;
        return owner->notify_ready();
    }, []() -> ii::core::Result<void> { return {}; }}));
    const auto result = app.exec();
    Backend::fail_close = false;
    check(!result && result.error().message == "Window closed callback failed" && result.error().cause &&
          Backend::scene_events.size() == 2 && app.event_loop().state() == ii::runtime::LoopState::stopped,
          "A window close callback failure did not reach the application result");
    tree->close();
}

void scene_open_failure_and_reentrant_close() {
    using Backend = NativeWindowBackend;
    Backend::scene_events.clear(); Backend::fail_open = true;
    auto owner = View::make_application_owner();
    ii::runtime::EventLoop loop;
    good(owner->bind_scene_loop(loop)); good(owner->activate());
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "open_failure", {640, 400}));
    good(host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, owner, 0); }));
    const auto failed = owner->scene_outcome();
    check(!failed && failed.error().message == "Window opened callback failed" && failed.error().cause &&
          loop.state() == ii::runtime::LoopState::stopped, "A window open failure was not stored or stopped the loop");
    const auto early_ready = owner->notify_ready();
    check(!early_ready && early_ready.error().message == "Window opened callback failed",
          "Application readiness ignored a failed window open event");
    Backend::fail_open = false; Backend::fail_close = true; NativeApplicationBackend::fail_stop = true;
    good(host.clear_content());
    const auto retired = owner->retire();
    Backend::fail_close = false; NativeApplicationBackend::fail_stop = false;
    check(!retired && retired.error().message == "Window opened callback failed" && Backend::scene_events.size() == 2,
          "Open failure was replaced during close or prevented cleanup");
    tree->close();

    Backend::scene_events.clear();
    auto nested_owner = View::make_application_owner();
    good(nested_owner->activate());
    auto nested_tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto nested_host = take(nested_tree->content_host({}, "nested_event", {640, 400}));
    Backend::during_open = [&] { good(nested_host.clear_content()); };
    const auto mounted = nested_host.set_content([&](auto& widgets) { return View::build_scene_with_application(widgets, {}, nested_owner, 0); });
    Backend::during_open = {};
    good(mounted);
    check(Backend::scene_events.size() == 2 && Backend::scene_events[0].starts_with("open:") &&
          Backend::scene_events[1].starts_with("close:") && nested_owner->scene_outcome(),
          "A window removed during its open callback lost or repeated its close event");
    good(nested_owner->retire()); nested_tree->close();
}

void native_callback_wiring() {
    auto host = std::make_shared<ii::gui::Host>(); // Does not open a native window.
    std::vector<std::weak_ptr<ii::gui::Host>> hosts{host};
    auto not_open = View::validate_scene_hosts(hosts);
    check(!not_open && not_open.error().code == ii::core::ErrorCode::closed,
          "An unopened native host was accepted as a ready scene");
    hosts.clear(); hosts.emplace_back();
    auto missing = View::validate_scene_hosts(hosts);
    check(!missing && missing.error().code == ii::core::ErrorCode::closed,
          "An expired native scene host was accepted as ready");
    ii::runtime::EventLoop loop;
    int ready = 0, attached = 0;
    auto startup = std::make_shared<lvui::native::SceneStartup>(2, [&]() -> ii::core::Result<void> { ++ready; return {}; });
    good(startup->mounted(0));
    auto callbacks = View::scene_callbacks(host, startup, loop, 1, [&](ii::gui::Host&) -> ii::core::Result<void> { ++attached; return {}; });
    callbacks.opened();
    check(attached == 1 && ready == 1 && startup->ready() && loop.state() != ii::runtime::LoopState::stopped,
          "The generated native callback did not complete startup");
    callbacks.opened();
    check(attached == 1 && ready == 1, "A late duplicate callback remounted a ready scene");
    callbacks.closed();
    check(startup->outcome() && loop.state() != ii::runtime::LoopState::stopped,
          "Closing a ready secondary window stopped the application");

    ii::runtime::EventLoop error_loop;
    auto failed = std::make_shared<lvui::native::SceneStartup>(2, []() -> ii::core::Result<void> { return {}; });
    auto failure = View::scene_callbacks(host, failed, error_loop, 1, [](ii::gui::Host&) -> ii::core::Result<void> {
        throw std::runtime_error("fixture attachment failure");
    });
    failure.opened();
    auto outcome = failed->outcome();
    check(!outcome && outcome.error().cause && error_loop.state() == ii::runtime::LoopState::stopped,
          "The native callback lost an asynchronous attachment exception");

    ii::runtime::EventLoop cancel_loop;
    int after_cancel = 0;
    auto canceled = std::make_shared<lvui::native::SceneStartup>(2, []() -> ii::core::Result<void> { return {}; });
    auto late = View::scene_callbacks(host, canceled, cancel_loop, 1, [&](ii::gui::Host&) -> ii::core::Result<void> { ++after_cancel; return {}; });
    good(canceled->cancel());
    late.opened(); late.closed();
    check(after_cancel == 0 && canceled->outcome() && cancel_loop.state() != ii::runtime::LoopState::stopped,
          "A callback mounted a scene after normal application cancellation");

    ii::runtime::EventLoop cancel_during_loop;
    auto cancel_during = std::make_shared<lvui::native::SceneStartup>(2, []() -> ii::core::Result<void> { return {}; });
    auto interrupted = View::scene_callbacks(host, cancel_during, cancel_during_loop, 1,
        [cancel_during](ii::gui::Host&) -> ii::core::Result<void> {
            good(cancel_during->cancel());
            throw std::runtime_error("error after cancellation");
        });
    interrupted.opened();
    auto interrupted_result = cancel_during->outcome();
    check(!interrupted_result && interrupted_result.error().cause && cancel_during_loop.state() == ii::runtime::LoopState::stopped,
          "An active attachment exception was hidden by cancellation");

    ii::runtime::EventLoop normal_cancel_loop;
    auto normal_cancel = std::make_shared<lvui::native::SceneStartup>(2, []() -> ii::core::Result<void> { return {}; });
    auto normal = View::scene_callbacks(host, normal_cancel, normal_cancel_loop, 1,
        [normal_cancel](ii::gui::Host&) -> ii::core::Result<void> { return normal_cancel->cancel(); });
    normal.opened();
    check(normal_cancel->outcome() && !normal_cancel->failed() && normal_cancel_loop.state() != ii::runtime::LoopState::stopped,
          "A successful attachment that canceled startup was reported as an error");

    ii::runtime::EventLoop early_loop;
    int after_close = 0;
    auto early = std::make_shared<lvui::native::SceneStartup>(2, []() -> ii::core::Result<void> { return {}; });
    auto closed = View::scene_callbacks(host, early, early_loop, 1, [&](ii::gui::Host&) -> ii::core::Result<void> { ++after_close; return {}; });
    closed.closed();
    closed.opened();
    check(early->failed() && !early->outcome() && after_close == 0 && early_loop.state() == ii::runtime::LoopState::stopped,
          "A scene closed before readiness without terminating startup safely");

    ii::runtime::EventLoop ready_loop;
    auto ready_failure = std::make_shared<lvui::native::SceneStartup>(2, []() -> ii::core::Result<void> {
        return ii::core::fail(ii::core::ErrorCode::failed, "ready rejected");
    });
    good(ready_failure->mounted(0));
    auto rejected = View::scene_callbacks(host, ready_failure, ready_loop, 1, [](ii::gui::Host&) -> ii::core::Result<void> { return {}; });
    rejected.opened();
    auto rejected_result = ready_failure->outcome();
    check(!rejected_result && rejected_result.error().message == "ready rejected" && ready_loop.state() == ii::runtime::LoopState::stopped,
          "An asynchronous ready failure was not returned through the host lifecycle");

    ii::runtime::EventLoop expired_loop;
    auto expired = std::make_shared<lvui::native::SceneStartup>(2, []() -> ii::core::Result<void> { return {}; });
    std::weak_ptr<ii::gui::Host> missing_host;
    auto lost = View::scene_callbacks(missing_host, expired, expired_loop, 1, [](ii::gui::Host&) -> ii::core::Result<void> { return {}; });
    lost.opened();
    auto missing_result = expired->outcome();
    check(!missing_result && missing_result.error().code == ii::core::ErrorCode::closed && expired_loop.state() == ii::runtime::LoopState::stopped,
          "An expired native host was not reported as a startup error");
}

void atomic_configuration() {
    if (!ii::gui::host_capabilities().secondary_windows) return;
    const auto before = NativeApplicationBackend::created;
    ii::runtime::Application app({}, {.workers={.workers=1}});
    good(app.attach({"scene/1", [](auto&) -> ii::core::Result<void> { return {}; },
                     []() -> ii::core::Result<void> { return {}; }}));
    auto configured = View::configure_application(app);
    check(!configured && configured.error().code == ii::core::ErrorCode::already_exists && NativeApplicationBackend::created == before,
          "A duplicate scene left a partial native application configuration");
    good(app.set_driver({[](const auto& start) { return start(); }, []() -> ii::core::Result<void> { return {}; }}));
    check(bool(app.exec()), "A failed scene configuration damaged existing application registrations");
}

void asynchronous_failure_reaches_application_result() {
    ii::runtime::Application app({}, {.workers={.workers=1}});
    auto host = std::make_shared<ii::gui::Host>();
    auto startup = std::make_shared<lvui::native::SceneStartup>(2, []() -> ii::core::Result<void> { return {}; });
    int cleanup_count = 0;
    auto callbacks = View::scene_callbacks(host, startup, app.event_loop(), 1, [](ii::gui::Host&) -> ii::core::Result<void> {
        return ii::core::fail(ii::core::ErrorCode::invalid_argument, "original scene failure");
    });
    good(app.attach({"main", [startup](auto&) { return startup->mounted(0); },
        [startup, &cleanup_count] { return startup->finish([&]() -> ii::core::Result<void> {
            ++cleanup_count; return ii::core::fail(ii::core::ErrorCode::failed, "main cleanup failed");
        }); }}));
    good(app.attach({"scene", [&app, opened=callbacks.opened](auto&) -> ii::core::Result<void> {
        if (!app.event_loop().post(opened)) return ii::core::fail(ii::core::ErrorCode::full, "Could not queue scene");
        return {};
    }, [startup, &cleanup_count] { return startup->finish([&]() -> ii::core::Result<void> {
        ++cleanup_count; return ii::core::fail(ii::core::ErrorCode::failed, "scene cleanup failed");
    }); }}));
    good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> {
        if (auto started = start(); !started) return started;
        (void)app.event_loop().poll();
        return {};
    }, []() -> ii::core::Result<void> { return {}; }}));
    auto result = app.exec();
    check(!result && result.error().code == ii::core::ErrorCode::invalid_argument &&
          result.error().message == "original scene failure" && cleanup_count == 2,
          "The app result lost the asynchronous scene failure or skipped cleanup");
}

void post_ready_host_failure_reaches_application_result() {
    ii::runtime::Application app({}, {.workers={.workers=1}});
    bool ready = false;
    auto startup = std::make_shared<lvui::native::SceneStartup>(2, [&]() -> ii::core::Result<void> { ready = true; return {}; });
    ii::gui::detail::HostFailure native_failure;
    native_failure.set_handler(View::scene_failure_handler(startup, app.event_loop()));
    int cleaned = 0;
    good(app.attach({"main", [startup](auto&) { return startup->mounted(0); },
        [startup, &cleaned] { return startup->finish([&]() -> ii::core::Result<void> { ++cleaned; return {}; }); }}));
    good(app.attach({"scene", [&](auto&) -> ii::core::Result<void> {
        if (auto mounted = startup->mounted(1); !mounted) return mounted;
        if (!app.event_loop().post([&] { native_failure = ii::core::Error{ii::core::ErrorCode::full, "native callback failed"}; }))
            return ii::core::fail(ii::core::ErrorCode::full, "Could not queue host failure");
        return {};
    }, [startup, &cleaned] { return startup->finish([&]() -> ii::core::Result<void> { ++cleaned; return {}; }); }}));
    good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> {
        if (auto started = start(); !started) return started;
        (void)app.event_loop().poll();
        return {};
    }, []() -> ii::core::Result<void> { return {}; }}));
    auto result = app.exec();
    check(ready && !result && result.error().code == ii::core::ErrorCode::full &&
          result.error().message == "native callback failed" && cleaned == 2,
          "A post-ready native host failure did not reach the application result");
}
} // namespace

int main() {
    independent_options(); scenes_share_only_application_state(); dynamic_scene_properties();
    scene_events_follow_publication(); scene_event_failure_reaches_application_result(); scene_open_failure_and_reentrant_close();
    native_callback_wiring(); atomic_configuration(); asynchronous_failure_reaches_application_result();
    post_ready_host_failure_reaches_application_result();
}
