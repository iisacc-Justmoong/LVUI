#include "../src/Runtime/SceneWindowSet.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>
#include <string>

namespace {
using Set = lvui::native::SceneWindowSet;
using Guard = lvui::native::WindowCloseGuard;
using ii::core::ErrorCode;
using ii::core::Result;

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
template<class T> T take(Result<T> value) {
    if (!value) throw std::runtime_error(value.error().message);
    return *value;
}

void independent_close_and_last_window_veto() {
    int approvals = 0;
    bool allow = false;
    Set scenes(2, [&]() -> Result<bool> { ++approvals; return allow; });
    auto first = take(scenes.activate(0));
    auto second = take(scenes.activate(1));
    check(take(scenes.request_close(first)) && approvals == 0 && take(scenes.active_count()) == 1,
          "Closing the first window asked to terminate the application");
    check(!take(scenes.request_close(second)) && approvals == 1 && take(scenes.active_count()) == 1,
          "A denied last-window request retired the window");
    allow = true;
    check(take(scenes.request_close(second)) && approvals == 2 && take(scenes.active_count()) == 0,
          "An approved last-window request did not retire the scene");
    check(!take(scenes.request_close(second)), "A closing window received a duplicate decision");
    check(!take(scenes.closed(first)) && !take(scenes.closed(second)),
          "An expected native close was treated as an unexpected empty application");
}

void generations_and_forced_closes() {
    Set scenes(2, []() -> Result<bool> { return true; });
    auto old = take(scenes.activate(0));
    auto other = take(scenes.activate(1));
    check(take(scenes.request_close(old)), "Could not close the old generation");
    check(!scenes.activate(0), "A new generation replaced one still closing");
    // The adapter confirmed native destruction, although the native close
    // notification itself is still queued behind a new open request.
    auto replacement = take(scenes.activate(0, true));
    check(!take(scenes.closed(old)) && take(scenes.active_count()) == 2,
          "A delayed old close retired its replacement");
    check(!take(scenes.closed(other)), "A forced close with another window open was considered empty");
    check(take(scenes.closed(replacement)) && take(scenes.active_count()) == 0,
          "The last forced close did not report an empty application");
    check(!take(scenes.closed(replacement)), "A repeated close reported an empty application twice");
    auto pending = take(scenes.activate(1));
    good(scenes.abandon(pending));
    check(take(scenes.active_count()) == 0 && !take(scenes.closed(pending)),
          "An abandoned creation remained active or produced a close event");

    Set forced(2, []() -> Result<bool> { return true; });
    auto vanished = take(forced.activate(0));
    auto sibling = take(forced.activate(1));
    // Native state reports the old host closed before its forced-close
    // notification has reached the application queue.
    auto reopened = take(forced.activate(0, true));
    check(take(forced.active_count()) == 2 && !take(forced.closed(vanished)) &&
          take(forced.request_close(reopened)) && take(forced.active_count()) == 1,
          "A forced-close notification damaged an already reopened generation");
    good(forced.abandon(sibling));
}

void reentrancy_and_failures() {
    std::unique_ptr<Set> scenes;
    Set::Ticket ticket{};
    bool nested_rejected = false, open_rejected = false, fail = true;
    scenes = std::make_unique<Set>(2, [&]() -> Result<bool> {
        nested_rejected = !take(scenes->request_close(ticket));
        auto open = scenes->activate(1);
        open_rejected = !open && open.error().code == ErrorCode::busy;
        if (fail) throw std::runtime_error("approval failed");
        good(scenes->cancel());
        return true;
    });
    ticket = take(scenes->activate(0));
    auto failed = scenes->request_close(ticket);
    check(!failed && failed.error().cause && nested_rejected && open_rejected && take(scenes->active_count()) == 1,
          "Approval failure or reentrancy damaged the active window");
    fail = false;
    check(take(scenes->request_close(ticket)) && take(scenes->active_count()) == 0,
          "Cancellation during an accepted approval damaged the active decision");
    check(!scenes->activate(1) && !take(scenes->closed(ticket)),
          "A retired application accepted a window or reported a forced close");
}

void ownership_and_bounds() {
    bool rejected_empty = false, rejected_excess = false;
    try { Set invalid(0, []() -> Result<bool> { return true; }); } catch (const std::invalid_argument&) { rejected_empty = true; }
    try { Set invalid(Set::maximum_scenes + 1, []() -> Result<bool> { return true; }); }
    catch (const std::invalid_argument&) { rejected_excess = true; }
    check(rejected_empty && rejected_excess, "Invalid application scene counts were accepted");
    Set scenes(1, []() -> Result<bool> { return true; });
    auto ticket = take(scenes.activate(0));
    check(!scenes.activate(1) && !scenes.activate(0), "An invalid or duplicate scene was accepted");
    bool close_wrong_thread = false, count_wrong_thread = false, abandon_wrong_thread = false;
    std::thread foreign([&] {
        auto close = scenes.request_close(ticket);
        auto count = scenes.active_count();
        auto abandon = scenes.abandon(ticket);
        close_wrong_thread = !close && close.error().code == ErrorCode::wrong_executor;
        count_wrong_thread = !count && count.error().code == ErrorCode::wrong_executor;
        abandon_wrong_thread = !abandon && abandon.error().code == ErrorCode::wrong_executor;
    });
    foreign.join();
    check(close_wrong_thread && count_wrong_thread && abandon_wrong_thread && take(scenes.active_count()) == 1,
          "A foreign thread changed application scene ownership");
    good(scenes.abandon(ticket));
}

void window_policies_and_global_quit() {
    std::vector<std::string> calls;
    bool first_allowed = false, second_allowed = true, app_allowed = true;
    auto first_guard = std::make_shared<Guard>();
    auto second_guard = std::make_shared<Guard>();
    good(first_guard->configure([&] { calls.emplace_back("first"); return first_allowed; }));
    good(second_guard->configure([&] { calls.emplace_back("second"); return second_allowed; }));
    good(first_guard->publish()); good(second_guard->publish());
    int last_requests = 0;
    Set scenes(2, [&]() -> Result<bool> { ++last_requests; calls.emplace_back("app"); return app_allowed; });
    auto first = take(scenes.activate(0, false, first_guard));
    auto second = take(scenes.activate(1, false, second_guard));
    check(!take(scenes.request_close(first)) && take(scenes.active_count()) == 2 &&
          calls == std::vector<std::string>{"first"} && last_requests == 0,
          "A window veto changed the application or asked for application approval");
    calls.clear(); first_allowed = true; second_allowed = false;
    auto approve_app = [&]() -> Result<bool> { ++last_requests; calls.emplace_back("app"); return app_allowed; };
    check(!take(scenes.request_application_close(approve_app)) && take(scenes.active_count()) == 2 &&
          calls == std::vector<std::string>({"first", "second"}) && last_requests == 0,
          "An application quit did not stop before the denied window");
    calls.clear(); second_allowed = true; app_allowed = false;
    check(!take(scenes.request_application_close(approve_app)) && take(scenes.active_count()) == 2 &&
          calls == std::vector<std::string>({"first", "second", "app"}) && last_requests == 1,
          "An application veto retired one or more windows");
    calls.clear(); app_allowed = true;
    check(take(scenes.request_application_close(approve_app)) && take(scenes.active_count()) == 2 &&
          calls == std::vector<std::string>({"first", "second", "app"}),
          "An approved application quit mutated windows before the host commits shutdown");
    calls.clear();
    check(take(scenes.request_close(first)) && calls == std::vector<std::string>{"first"},
          "An ordinary window close did not use its own policy");
    calls.clear();
    check(take(scenes.request_close(second)) && calls == std::vector<std::string>({"second", "app"}),
          "The last window did not ask its own policy before the application policy");
}

void close_policy_reentrancy_and_generation_changes() {
    auto guard = std::make_shared<Guard>();
    Set scenes(2, []() -> Result<bool> { return true; });
    Set::Ticket first{};
    bool nested_close = true, nested_quit = true, mutate = false, fail = false;
    good(guard->configure([&] {
        nested_close = take(scenes.request_close(first));
        nested_quit = take(scenes.request_application_close([]() -> Result<bool> { return true; }));
        if (mutate) (void)take(scenes.closed(first));
        if (fail) throw std::runtime_error("window veto failure");
        return true;
    }));
    good(guard->publish());
    first = take(scenes.activate(0, false, guard));
    (void)take(scenes.activate(1));
    fail = true;
    auto failed = scenes.request_close(first);
    check(!failed && failed.error().cause && !nested_close && !nested_quit && take(scenes.active_count()) == 2,
          "A failing or reentrant window decision damaged scene state");
    fail = false; mutate = true;
    int application_calls = 0;
    check(!take(scenes.request_application_close([&]() -> Result<bool> { ++application_calls; return true; })) &&
          application_calls == 0 && take(scenes.active_count()) == 1,
          "Application quit continued after a scene changed during approval");
    good(guard->retire());
    check(!take(guard->request_close()), "A retired window policy was invoked");

    Set application_mutation(2, []() -> Result<bool> { return true; });
    auto old = take(application_mutation.activate(0));
    (void)take(application_mutation.activate(1));
    check(!take(application_mutation.request_application_close([&]() -> Result<bool> {
        (void)take(application_mutation.closed(old)); return true;
    })), "Application approval committed after a window generation changed");
}

void close_policy_publication_and_ownership() {
    auto guard = std::make_shared<Guard>();
    int calls = 0;
    check(take(guard->request_close()), "An unpublished window was not allowed to close");
    good(guard->configure([&] {
        ++calls;
        check(!take(guard->request_close()), "A nested window policy request was accepted");
        return true;
    }));
    check(take(guard->request_close()) && calls == 0, "A callback ran before its window was published");
    good(guard->publish());
    check(take(guard->request_close()) && calls == 1, "A published window policy did not run");
    bool wrong_owner = false;
    std::thread foreign([&] {
        auto result = guard->request_close();
        wrong_owner = !result && result.error().code == ErrorCode::wrong_executor;
    });
    foreign.join();
    check(wrong_owner && calls == 1, "A foreign thread ran the window policy");
    good(guard->retire());
    check(!take(guard->request_close()) && !guard->publish(), "A retired policy was reused");
    good(guard->prepare());
    good(guard->configure([&] { ++calls; return true; }));
    good(guard->publish());
    check(take(guard->request_close()) && calls == 2 && !guard->prepare(),
          "A new window activation did not replace its retired policy safely");
    good(guard->retire());

    auto closing = std::make_shared<Guard>();
    good(closing->configure([&] { good(closing->retire()); return true; }));
    good(closing->publish());
    check(!take(closing->request_close()), "A policy retired during its callback still approved close");

    Set scenes(1, []() -> Result<bool> { return true; });
    auto owned = std::make_shared<Guard>();
    std::weak_ptr<Guard> weak = owned;
    auto ticket = take(scenes.activate(0, false, owned));
    owned.reset();
    good(scenes.abandon(ticket));
    check(weak.expired(), "An abandoned scene retained its close policy");
    owned = std::make_shared<Guard>(); weak = owned;
    ticket = take(scenes.activate(0, false, owned));
    owned.reset();
    check(take(scenes.closed(ticket)) && weak.expired(), "A closed scene retained its close policy");
}
} // namespace

int main() {
    try {
        independent_close_and_last_window_veto();
        generations_and_forced_closes();
        reentrancy_and_failures();
        ownership_and_bounds();
        window_policies_and_global_quit();
        close_policy_reentrancy_and_generation_changes();
        close_policy_publication_and_ownership();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
