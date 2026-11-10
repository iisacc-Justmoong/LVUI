#include "LVUINativeApplicationFixture.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

namespace {
using View = lvui_generated::LVUINativeApplicationFixture;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void good(ii::core::Result<void> result) {
    if (!result) {
        if (result.error().cause) try { std::rethrow_exception(result.error().cause); }
        catch (const std::exception& error) { throw std::runtime_error(result.error().message + ": " + error.what()); }
        throw std::runtime_error(result.error().message);
    }
}

void configuration() {
    const auto before = NativeApplicationBackend::created;
    {
        ii::runtime::Application app;
        good(View::configure_application(app));
        const auto duplicate = View::configure_application(app);
        check(!duplicate && duplicate.error().code == ii::core::ErrorCode::already_exists,
              "A second application driver was accepted");
        check(NativeApplicationBackend::created == before, "Configuration activated a backend before the view opened");
    }
    {
        ii::runtime::Application app;
        good(app.attach({"main", [](auto&) -> ii::core::Result<void> { return {}; },
                         []() -> ii::core::Result<void> { return {}; }}));
        const auto failed = View::configure_application(app);
        check(!failed && failed.error().code == ii::core::ErrorCode::already_exists,
              "Duplicate main attachment was accepted");
        check(NativeApplicationBackend::created == before, "Failed configuration activated the backend");
        good(app.set_driver({[](const auto& start) { return start(); },
                             []() -> ii::core::Result<void> { return {}; }}));
        check(bool(app.exec()), "Failed generated configuration left a driver installed");
    }
}

void scope_lifetime() {
    const auto before_created = NativeApplicationBackend::created;
    const auto before_destroyed = NativeApplicationBackend::destroyed;
    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "root", {640, 400}));
    std::shared_ptr<View::Scope> scope;
    auto factories = lvui::native::BackendFactories{}.with<NativeApplicationBackend>([] {
        return NativeApplicationBackend("Injected");
    });
    good(host.set_content([&](auto& widgets) { return View::build(widgets, {}, {}, &scope, factories); }));
    check(scope && scope->backend_0.title == "Injected" && NativeApplicationBackend::created == before_created + 1,
          "Application backend factory or activation ownership was lost");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content());
    scope.reset();
    check(weak.expired() && NativeApplicationBackend::destroyed == before_destroyed + 1,
          "Application backend was not retired exactly once");
    state->close();
}

#if defined(IISDKCORE_APPLICATION_DRIVER_STOPPED_API) && IISDKCORE_APPLICATION_DRIVER_STOPPED_API >= 1
void application_driver_stopped_before_loop_return() {
    using Backend = NativeApplicationBackend;
    Backend::events.clear();
    Backend::scene_events.clear();
    Backend::stopped_after_window = false;
    const auto app_created = Backend::created;
    const auto app_destroyed = Backend::destroyed;
    const auto window_created = NativeWindowBackend::created;
    const auto window_destroyed = NativeWindowBackend::destroyed;
    auto owner = View::make_application_owner();
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "driver_stop", {640, 400}));
    ii::runtime::Application app({}, {.workers = {.workers = 1}});
    good(app.attach_before_driver(View::application_attachment("application", owner)));
    good(app.attach({"window", [&](auto&) -> ii::core::Result<void> {
        if (auto opened = host.set_content([&](auto& widgets) {
            return View::build_with_application(widgets, {}, owner);
        }); !opened) return opened;
        return owner->notify_ready();
    }, [&]() -> ii::core::Result<void> { return host.clear_content(); }}));
    bool checked_before_driver_return = false;
    good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> {
        good(start());
        check(Backend::events == std::vector<std::string>({"start", "ready"}) &&
              Backend::scene_events == std::vector<std::string>{"opened"} &&
              NativeWindowBackend::created == window_created + 1,
              "Generated LVUI application did not start its window");
        good(app.driver_stopped());
        check(app.state() == ii::runtime::ApplicationState::stopped && !owner->scope() &&
              Backend::events == std::vector<std::string>({"start", "ready", "stop"}) &&
              Backend::scene_events == std::vector<std::string>({"opened", "closed"}) &&
              Backend::stopped_after_window && NativeWindowBackend::destroyed == window_destroyed + 1 &&
              Backend::created == app_created + 1 && Backend::destroyed == app_destroyed + 1,
              "Generated LVUI owners did not retire in window-before-application order");
        good(app.driver_stopped());
        check(Backend::events == std::vector<std::string>({"start", "ready", "stop"}),
              "Repeated driver completion delivered onStop more than once");
        checked_before_driver_return = true;
        return {};
    }, []() -> ii::core::Result<void> { return {}; }}));
    check(bool(app.exec()), "Application driver completion failed");
    check(checked_before_driver_return && Backend::destroyed == app_destroyed + 1,
          "Driver return repeated or skipped application retirement");
    tree->close();
}
#endif

void single_window_scene_events() {
    using Backend = NativeApplicationBackend;
    Backend::scene_events.clear();
    auto owner = View::make_application_owner();
    good(owner->activate());
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "single_scene", {640, 400}));
    auto other_tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto other_host = take(other_tree->content_host({}, "other_scene", {640, 400}));
    good(host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }));
    good(other_host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }));
    check(Backend::scene_events == std::vector<std::string>({"opened", "opened"}), "Window publication lost the app-scoped events");
    good(owner->notify_ready());
    Backend::during_scene_close = [&](Backend& vm) {
        vm.title = "Updated during close";
        auto scope = owner->scope();
        scope->revision_0.set(scope->revision_0.get() + 1);
        check(take(scope->binding_failures()).empty(), "A retiring nested view still applied presentation effects");
    };
    good(host.clear_content());
    Backend::during_scene_close = {};
    good(owner->scene_outcome());
    const auto nodes = take(other_tree->snapshots());
    check(std::ranges::any_of(nodes, [](const auto& node) { return node.id.ends_with("app_label") && node.text == "Updated during close"; }),
          "An app-scoped window close event did not update another live view");
    good(other_host.clear_content());
    check(Backend::scene_events == std::vector<std::string>({"opened", "opened", "closed", "closed"}), "Window retirement lost an app-scoped event");
    Backend::scene_events.clear();
    good(host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }));
    good(other_host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }));
    bool nested = false;
    Backend::during_scene_close = [&](Backend&) { if (!std::exchange(nested, true)) good(other_host.clear_content()); };
    good(host.clear_content());
    Backend::during_scene_close = {};
    check(nested && Backend::scene_events == std::vector<std::string>({"opened", "opened", "closed", "closed"}) && owner->scene_outcome(),
          "A nested window close lost or repeated a scene event");
    good(owner->retire()); tree->close(); other_tree->close();
}

void application_scope_lifetime() {
    NativeApplicationBackend::events.clear();
    NativeApplicationBackend::stopped_after_window=false;
    const auto app_created=NativeApplicationBackend::created;
    const auto app_destroyed=NativeApplicationBackend::destroyed;
    const auto window_created=NativeWindowBackend::created;
    const auto window_destroyed=NativeWindowBackend::destroyed;
    auto factories=lvui::native::BackendFactories{}.with<NativeApplicationBackend>([] {
        return NativeApplicationBackend("App state");
    });
    auto owner=View::make_application_owner({},factories);
    bool start_reentry=false, ready_reentry=false, stop_reentry=false;
    NativeApplicationBackend::during_start=[&] { auto result=owner->retire();
        start_reentry=!result && result.error().code==ii::core::ErrorCode::busy; };
    NativeApplicationBackend::during_ready=[&] { auto result=owner->notify_ready();
        ready_reentry=!result && result.error().code==ii::core::ErrorCode::busy; };
    NativeApplicationBackend::during_stop=[&] { auto result=owner->retire();
        stop_reentry=!result && result.error().code==ii::core::ErrorCode::busy; };
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host=take(tree->content_host({},"root",{640,400}));
    auto open=[&]() { return host.set_content([&](auto& widgets) {
        return View::build_with_application(widgets,{},owner);
    }); };
    auto too_early=open();
    check(!too_early && too_early.error().code==ii::core::ErrorCode::closed,
          "A view borrowed an inactive application scope");
    auto ready_too_early=owner->notify_ready();
    check(!ready_too_early && ready_too_early.error().code==ii::core::ErrorCode::closed,
          "An inactive application was declared ready");

    ii::runtime::Application app({}, {.workers={.workers=1}});
    good(app.attach_before_driver(View::application_attachment("application",owner)));
    good(app.attach({"window",[&](auto&) -> ii::core::Result<void> {
                         if (auto opened=open(); !opened) return opened;
                         return owner->notify_ready(); },
                     [&]() -> ii::core::Result<void> { return host.clear_content(); }}));
    std::weak_ptr<View::Scope> application_scope;
    good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> {
        check(NativeApplicationBackend::created==app_created+1 && NativeWindowBackend::created==window_created,
              "Application backend did not activate before the native driver");
        check(NativeApplicationBackend::events==std::vector<std::string>{"start"},
              "Application start callback did not precede the driver");
        auto scope=owner->scope();
        check(bool(scope),"Active application scope is unavailable");
        auto without_window=owner->notify_ready();
        check(!without_window && without_window.error().code==ii::core::ErrorCode::closed,
              "Application readiness was delivered before a window attached");
        application_scope=scope;
        auto* original=std::addressof(scope->backend_0);
        good(start());
        check(NativeApplicationBackend::events==std::vector<std::string>({"start","ready"}),
              "Application ready callback did not follow the window attachment");
        const auto ready_nodes=take(tree->snapshots());
        check(std::ranges::any_of(ready_nodes,[](const auto& node) {
            return node.id.ends_with("app_label") && node.text=="Ready app state";
        }),"The ready callback did not publish its application state to the window");
        auto duplicate=owner->notify_ready();
        check(!duplicate && duplicate.error().code==ii::core::ErrorCode::busy,
              "Application delivered the ready callback more than once");
        check(NativeWindowBackend::created==window_created+1 && NativeApplicationBackend::created==app_created+1,
              "The first window did not borrow the existing application backend");
        auto premature=owner->retire();
        check(!premature && premature.error().code==ii::core::ErrorCode::busy && owner->scope(),
              "Application scope retired while a window still borrowed it");
        bool wrong_thread=false;
        std::thread worker([&] { auto result=owner->retire();
            wrong_thread=!result && result.error().code==ii::core::ErrorCode::wrong_executor; });
        worker.join();
        check(wrong_thread, "A foreign thread retired the application scope");
        bool wrong_ready_thread=false;
        std::thread ready_worker([&] { auto result=owner->notify_ready();
            wrong_ready_thread=!result && result.error().code==ii::core::ErrorCode::wrong_executor; });
        ready_worker.join();
        check(wrong_ready_thread,"A foreign thread delivered application readiness");
        good(host.clear_content());
        check(NativeWindowBackend::destroyed==window_destroyed+1 &&
              NativeApplicationBackend::destroyed==app_destroyed && !application_scope.expired(),
              "Closing a window retired the application scope");
        good(open());
        check(NativeWindowBackend::created==window_created+2 && std::addressof(scope->backend_0)==original,
              "Reopening the window recreated application state");
        scope->backend_0.title="Changed app state";
        scope->publish_native_changes();
        const auto nodes=take(tree->snapshots());
        check(std::ranges::any_of(nodes,[](const auto& node) {
            return node.id.ends_with("app_label") && node.text=="Changed app state";
        }),"A window did not observe its shared application state");
        return {};
    },[]() -> ii::core::Result<void> { return {}; }}));
    check(bool(app.exec()),"Application lifecycle failed");
    check(application_scope.expired() && NativeApplicationBackend::destroyed==app_destroyed+1 &&
          NativeWindowBackend::destroyed==window_destroyed+2,
          "Application and window scopes did not retire independently");
    check(NativeApplicationBackend::events==std::vector<std::string>({"start","ready","stop"}) &&
          NativeApplicationBackend::stopped_after_window && start_reentry && ready_reentry && stop_reentry,
          "Application stop callback did not follow window retirement");
    NativeApplicationBackend::during_start={}; NativeApplicationBackend::during_ready={};
    NativeApplicationBackend::during_stop={};
    auto after_stop=open();
    check(!after_stop && after_stop.error().code==ii::core::ErrorCode::closed,
          "A stopped application scope was reused");
    auto late_ready=owner->notify_ready();
    check(!late_ready && late_ready.error().code==ii::core::ErrorCode::closed,
          "A stopped application reported readiness as merely duplicated");
    tree->close();
}

void standalone_stop_failure() {
    using Backend=NativeApplicationBackend;
    const auto destroyed=Backend::destroyed;
    Backend::events.clear(); Backend::fail_stop=true;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host=take(tree->content_host({},"root",{640,400}));
    good(host.set_content([&](auto& widgets) { return View::build(widgets); }));
    good(host.clear_content());
    check(Backend::events==std::vector<std::string>({"start","stop"}) && Backend::destroyed==destroyed+1,
          "Standalone retirement failed to clean up after a stop callback exception");
    Backend::fail_stop=false;
    tree->close();
}

void activity_lifecycle() {
    using Backend=NativeApplicationBackend;
    Backend::events.clear();
    auto owner=View::make_application_owner();
    auto too_early=owner->notify_activity(true);
    check(!too_early && too_early.error().code==ii::core::ErrorCode::closed,
          "An inactive application accepted an activity transition");
    good(owner->activate());
    good(owner->notify_activity(false));
    good(owner->notify_activity(true));
    check(Backend::events==std::vector<std::string>{"start"},"Activity escaped before readiness");
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host=take(tree->content_host({},"root",{640,400}));
    good(host.set_content([&](auto& widgets) { return View::build_with_application(widgets,{},owner); }));
    good(owner->notify_ready());
    check(Backend::events==std::vector<std::string>({"start","ready","active"}),
          "The last pre-ready activity was not delivered after readiness");
    good(owner->notify_activity(true));
    check(Backend::events.size()==3,"Duplicate activity was delivered");
    good(owner->notify_activity(false));
    bool injected=false;
    Backend::during_active=[&] {
        if (!injected) {
            injected=true;
            good(owner->notify_activity(false));
            good(owner->notify_activity(true));
            good(owner->notify_activity(false));
        }
    };
    good(owner->notify_activity(true));
    Backend::during_active={};
    check(Backend::events==std::vector<std::string>({"start","ready","active","inactive","active","inactive","active","inactive"}),
          "Reentrant activity did not preserve callback ordering");
    bool wrong_thread=false;
    std::thread worker([&] { auto result=owner->notify_activity(true);
        wrong_thread=!result && result.error().code==ii::core::ErrorCode::wrong_executor; });
    worker.join();
    check(wrong_thread,"A foreign thread delivered application activity");
    Backend::fail_active=true;
    auto failed=owner->notify_activity(true);
    check(!failed && failed.error().cause,"An activity callback exception was lost");
    Backend::fail_active=false;
    auto after_failure=owner->notify_activity(false);
    check(!after_failure && after_failure.error().code==ii::core::ErrorCode::closed,
          "Activity delivery continued after a callback failed");
    good(host.clear_content());
    good(owner->retire());
    auto late=owner->notify_activity(false);
    check(!late && late.error().code==ii::core::ErrorCode::closed,
          "A stopped application accepted an activity transition");
    tree->close();
}

void activity_reentry_limit() {
    using Backend=NativeApplicationBackend;
    Backend::events.clear();
    auto owner=View::make_application_owner();
    good(owner->activate());
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host=take(tree->content_host({},"root",{640,400}));
    good(host.set_content([&](auto& widgets) { return View::build_with_application(widgets,{},owner); }));
    good(owner->notify_ready());
    Backend::during_active=[&] { good(owner->notify_activity(false)); };
    Backend::during_inactive=[&] { good(owner->notify_activity(true)); };
    auto limited=owner->notify_activity(true);
    Backend::during_active={}; Backend::during_inactive={};
    check(!limited && limited.error().code==ii::core::ErrorCode::full && Backend::events.size()==66,
          "Unbounded reentrant activity was not rejected");
    good(host.clear_content());
    good(owner->retire());
    tree->close();
}

void activity_queue_limit() {
    using Backend=NativeApplicationBackend;
    auto owner=View::make_application_owner();
    good(owner->activate());
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host=take(tree->content_host({},"root",{640,400}));
    good(host.set_content([&](auto& widgets) { return View::build_with_application(widgets,{},owner); }));
    good(owner->notify_ready());
    Backend::during_active=[&] {
        for (int i=0;i<65;++i) (void)owner->notify_activity((i%2)!=0);
    };
    auto limited=owner->notify_activity(true);
    Backend::during_active={};
    check(!limited && limited.error().code==ii::core::ErrorCode::full,
          "An ignored reentrant queue overflow did not reach the host");
    good(host.clear_content());
    good(owner->retire());
    tree->close();
}

void file_requests_follow_app_lifecycle() {
    using Backend = NativeApplicationBackend;
    Backend::events.clear(); Backend::file_events.clear();
    auto owner = View::make_application_owner();
    check(!owner->notify_files({"/too-early"}), "An inactive app accepted a file request");
    good(owner->activate());
    auto native_delivery = View::file_open_handler(owner);
    native_delivery({"/first", "/첫 문서.iisc"});
    native_delivery({"/second"});
    check(Backend::file_events.empty(), "File requests ran before app readiness");
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "file_request", {640, 400}));
    good(host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }));
    Backend::during_ready = [&] { good(owner->notify_files({"/during-ready"})); };
    good(owner->notify_ready());
    Backend::during_ready = {};
    check(Backend::file_events == std::vector<std::vector<std::string>>({{"/first", "/첫 문서.iisc"}, {"/second"}, {"/during-ready"}}),
          "File requests did not wait for readiness or preserve their batches");
    auto nodes = take(tree->snapshots());
    check(std::ranges::any_of(nodes, [](const auto& node) { return node.id.ends_with("app_label") && node.text == "/during-ready"; }),
          "The file handler did not publish its app state to the window");
    Backend::events.clear();
    bool injected_activity = false, injected_files = false;
    Backend::during_files = [&] { if (!std::exchange(injected_activity, true)) good(owner->notify_activity(false)); };
    Backend::during_inactive = [&] { if (!std::exchange(injected_files, true)) good(owner->notify_files({"/after-inactive"})); };
    good(owner->notify_files({"/cross-event"}));
    Backend::during_files = {}; Backend::during_inactive = {};
    check(Backend::events == std::vector<std::string>({"files", "inactive", "files"}) &&
          Backend::file_events.back() == std::vector<std::string>{"/after-inactive"},
          "Cross-lifecycle reentry lost or recursively delivered a file request");
    bool wrong = false;
    std::thread worker([&] { auto result = owner->notify_files({"/foreign"}); wrong = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); check(wrong, "A foreign thread delivered application files");
    Backend::fail_files = true;
    bool propagated = false;
    try { native_delivery({"/fails"}); } catch (const std::exception&) { propagated = true; }
    Backend::fail_files = false;
    check(propagated && !owner->notify_files({"/late"}), "A file callback error was hidden or delivery continued");
    good(host.clear_content());
    auto retired = owner->retire();
    check(!retired && retired.error().cause, "Application cleanup lost a file callback failure");
    tree->close();
}

void file_requests_are_discarded_after_failed_readiness() {
    using Backend = NativeApplicationBackend;
    Backend::file_events.clear(); Backend::fail_ready = true;
    auto owner = View::make_application_owner(); good(owner->activate());
    good(owner->notify_files({"/pending"}));
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "failed_file_request", {640, 400}));
    good(host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }));
    auto failed = owner->notify_ready();
    Backend::fail_ready = false;
    check(!failed && failed.error().cause && Backend::file_events.empty() && !owner->notify_files({"/after-failure"}),
          "A failed app readiness delivered or admitted a file request");
    good(host.clear_content()); good(owner->retire()); tree->close();
    check(Backend::file_events.empty(), "App retirement delivered a pending file request");
}

void cross_event_reentry_is_bounded() {
    using Backend = NativeApplicationBackend;
    Backend::events.clear(); Backend::file_events.clear();
    auto owner = View::make_application_owner(); good(owner->activate());
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "cross_event_limit", {640, 400}));
    good(host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }));
    good(owner->notify_ready());
    bool next = false;
    Backend::during_files = [&] { next = !next; good(owner->notify_activity(next)); };
    Backend::during_active = Backend::during_inactive = [&] { good(owner->notify_files({"/again"})); };
    auto limited = owner->notify_files({"/first"});
    Backend::during_files = {}; Backend::during_active = {}; Backend::during_inactive = {};
    check(!limited && limited.error().code == ii::core::ErrorCode::full && Backend::file_events.size() == 64,
          "Cross-event reentry was not bounded or its error was lost");
    good(host.clear_content());
    auto retired = owner->retire();
    check(!retired && retired.error().code == ii::core::ErrorCode::full, "Cross-event error was lost during cleanup");
    tree->close();
}

void file_failure_reaches_core_application() {
    using Backend = NativeApplicationBackend;
    Backend::events.clear(); Backend::file_events.clear(); Backend::fail_files = true;
    auto owner = View::make_application_owner();
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "file_failure_app", {640, 400}));
    ii::runtime::Application app({}, {.workers={.workers=1}});
    good(app.attach_before_driver(View::application_attachment("application", owner)));
    good(app.attach({"window", [&](auto&) -> ii::core::Result<void> {
        if (auto mounted = host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }); !mounted) return mounted;
        return owner->notify_ready();
    }, [&]() -> ii::core::Result<void> { return host.clear_content(); }}));
    good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> {
        if (auto started = start(); !started) return started;
        View::file_open_handler(owner)({"/broken"});
        return {};
    }, []() -> ii::core::Result<void> { return {}; }}));
    auto result = app.exec(); Backend::fail_files = false;
    check(!result && result.error().cause && app.state() == ii::runtime::ApplicationState::stopped &&
          Backend::events == std::vector<std::string>({"start", "ready", "files", "stop"}),
          "The Core application hid the file callback error or skipped cleanup");
    tree->close();
}

void close_request_cancels_file_admission() {
    using Backend = NativeApplicationBackend;
    Backend::file_events.clear();
    auto owner = View::make_application_owner(); good(owner->activate());
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "file_close_request", {640, 400}));
    good(host.set_content([&](auto& widgets) { return View::build_with_application(widgets, {}, owner); }));
    good(owner->notify_ready());
    auto native_delivery = View::file_open_handler(owner);
    Backend::during_files = [&] {
        good(owner->notify_files({"/queued"}));
        good(owner->cancel_file_events());
        check(!owner->notify_files({"/late"}), "A closing app admitted a file request");
        native_delivery({"/late-native"});
    };
    good(owner->notify_files({"/current"}));
    Backend::during_files = {};
    check(Backend::file_events == std::vector<std::vector<std::string>>({{"/current"}}),
          "A closing app delivered a queued file request");
    good(host.clear_content()); good(owner->retire()); tree->close();
}

void lifecycle_failures() {
    using Backend=NativeApplicationBackend;
    auto run=[](bool fail_start, bool fail_ready, bool fail_stop) {
        Backend::events.clear(); Backend::fail_start=fail_start; Backend::fail_ready=fail_ready;
        Backend::fail_stop=fail_stop; Backend::stopped_after_window=false;
        const auto created=Backend::created, destroyed=Backend::destroyed;
        auto owner=View::make_application_owner();
        auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
        auto host=take(tree->content_host({},"root",{640,400}));
        ii::runtime::Application app({}, {.workers={.workers=1}});
        bool entered=false;
        good(app.attach_before_driver(View::application_attachment("application",owner)));
        good(app.attach({"window",[&](auto&) -> ii::core::Result<void> {
            if (auto opened=host.set_content([&](auto& widgets) { return View::build_with_application(widgets,{},owner); }); !opened) return opened;
            return owner->notify_ready();
        },[&]() -> ii::core::Result<void> { return host.clear_content(); }}));
        good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> { entered=true; return start(); },
                             []() -> ii::core::Result<void> { return {}; }}));
        const auto result=app.exec();
        check(!result && result.error().cause && entered!=fail_start && !owner->scope(),
              "Lifecycle callback failure lost the original error or application cleanup");
        try { std::rethrow_exception(result.error().cause); }
        catch (const std::runtime_error& error) {
            const std::string_view expected=fail_start ? "start callback failure" :
                fail_ready ? "ready callback failure" : "stop callback failure";
            check(error.what()==expected,"Lifecycle cleanup replaced the first callback error");
        }
        check(Backend::created==created+1 && Backend::destroyed==destroyed+1,
              "Lifecycle callback failure leaked an application backend");
        if (fail_start)
            check(Backend::events==std::vector<std::string>{"start"},"Failed start delivered later callbacks");
        else
            check(Backend::events==std::vector<std::string>({"start","ready","stop"}) && Backend::stopped_after_window,
                  "Lifecycle failure skipped stop or left the window active");
        Backend::fail_start=Backend::fail_ready=Backend::fail_stop=false;
        tree->close();
    };
    run(true,false,false);
    run(false,true,false);
    run(false,false,true);
    run(false,true,true);
}

void failed_application_start() {
    auto factories=lvui::native::BackendFactories{}.with<NativeApplicationBackend>([]() -> NativeApplicationBackend {
        throw std::runtime_error("application construction failed");
    });
    auto owner=View::make_application_owner({},factories);
    ii::runtime::Application app({}, {.workers={.workers=1}});
    bool driver_entered=false;
    good(app.attach_before_driver(View::application_attachment("application",owner)));
    good(app.set_driver({[&](const auto&) -> ii::core::Result<void> { driver_entered=true; return {}; },
                         []() -> ii::core::Result<void> { return {}; }}));
    const auto result=app.exec();
    check(!result && result.error().cause && !driver_entered && !owner->scope(),
          "Failed application construction entered the native driver or retained state");
}

void failed_window_start() {
    const auto before_created=NativeApplicationBackend::created;
    const auto before_destroyed=NativeApplicationBackend::destroyed;
    auto factories=lvui::native::BackendFactories{}.with<NativeWindowBackend>([]() -> NativeWindowBackend {
        throw std::runtime_error("window construction failed");
    });
    auto owner=View::make_application_owner({},factories);
    good(owner->activate());
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host=take(tree->content_host({},"root",{640,400}));
    auto failed=host.set_content([&](auto& widgets) { return View::build_with_application(widgets,{},owner); });
    check(!failed && owner->scope() && NativeApplicationBackend::created==before_created+1 &&
          NativeApplicationBackend::destroyed==before_destroyed,
          "A failed window construction retired its application scope");
    good(owner->retire());
    check(NativeApplicationBackend::destroyed==before_destroyed+1,
          "A failed window construction retained an application window lease");
    tree->close();
}
}

int main() {
    configuration();
    scope_lifetime();
    single_window_scene_events();
    application_scope_lifetime();
#if defined(IISDKCORE_APPLICATION_DRIVER_STOPPED_API) && IISDKCORE_APPLICATION_DRIVER_STOPPED_API >= 1
    application_driver_stopped_before_loop_return();
#endif
    failed_application_start();
    failed_window_start();
    lifecycle_failures();
    standalone_stop_failure();
    activity_lifecycle();
    activity_reentry_limit();
    activity_queue_limit();
    file_requests_follow_app_lifecycle();
    file_requests_are_discarded_after_failed_readiness();
    cross_event_reentry_is_bounded();
    file_failure_reaches_core_application();
    close_request_cancels_file_admission();
}
