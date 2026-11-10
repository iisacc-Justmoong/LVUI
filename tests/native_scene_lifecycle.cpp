#include <LVUI/SceneLifecycle.h>
#include <LVUI/SceneEffectGroup.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(ii::core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }

void delivery_and_cancellation() {
    std::vector<std::string> events;
    lvui::native::SceneLifecycle lifecycle;
    good(lifecycle.configure([&] { events.emplace_back("opened"); }, [&] { events.emplace_back("closed"); }));
    good(lifecycle.opened()); good(lifecycle.opened());
    good(lifecycle.closed()); good(lifecycle.closed()); good(lifecycle.opened());
    check(events == std::vector<std::string>({"opened", "closed"}), "Scene events repeated or changed order");
    check(!lifecycle.configure({}, {}), "A retired scene accepted new events");

    lvui::native::SceneLifecycle unpublished;
    good(unpublished.configure([&] { events.emplace_back("unexpected open"); }, [&] { events.emplace_back("unexpected close"); }));
    good(unpublished.closed()); good(unpublished.opened());
    check(events.size() == 2, "An unpublished scene delivered a lifecycle event");
}

void nested_close_and_failures() {
    std::vector<std::string> events;
    lvui::native::SceneLifecycle lifecycle;
    good(lifecycle.configure([&] { events.emplace_back("opening"); good(lifecycle.closed()); events.emplace_back("opened"); },
                             [&] { events.emplace_back("closed"); good(lifecycle.closed()); }));
    good(lifecycle.opened());
    check(events == std::vector<std::string>({"opening", "closed", "opened"}), "Reentrant close was lost or repeated");

    lvui::native::SceneLifecycle failures;
    good(failures.configure([] { throw std::runtime_error("opening error"); }, [] { throw std::runtime_error("closing error"); }));
    auto open = failures.opened();
    auto close = failures.closed();
    check(!open && open.error().cause && open.error().message == "Window opened callback failed" &&
          !close && close.error().cause && close.error().message == "Window closed callback failed",
          "Scene callback exceptions were lost");
    good(failures.closed());
}

void owner_thread() {
    lvui::native::SceneLifecycle lifecycle;
    good(lifecycle.configure({}, {}));
    bool rejected = false;
    std::thread other([&] { auto result = lifecycle.opened(); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    other.join();
    check(rejected, "Another thread delivered a scene event");
    good(lifecycle.opened()); good(lifecycle.closed());
}

void scoped_effect_groups() {
    ii::core::Observable<int> input{1};
    auto member = std::make_shared<lvui::native::Bindings>();
    auto already_closed = std::make_shared<lvui::native::Bindings>();
    auto other_window = std::make_shared<lvui::native::Bindings>();
    auto derived = member->derive([&](lvui::native::ReadContext& context) { return context.read(input) * 2; });
    check(bool(derived), "Cannot create grouped derived value");
    int member_value = 0, other_value = 0;
    check(bool(member->bind([&](lvui::native::ReadContext& context) { return context.read(input); },
                            [&](int value) -> ii::core::Result<void> { member_value = value; return {}; })), "Cannot create grouped effect");
    check(bool(other_window->bind([&](lvui::native::ReadContext& context) { return context.read(input); },
                                  [&](int value) -> ii::core::Result<void> { other_value = value; return {}; })), "Cannot create other window effect");
    lvui::native::SceneEffectGroup group;
    good(group.track(member)); good(group.track(already_closed)); good(already_closed->close());
    std::weak_ptr<lvui::native::Bindings> temporary;
    { auto graph = std::make_shared<lvui::native::Bindings>(); temporary = graph; good(group.track(graph)); }
    check(temporary.expired(), "A window group retained a child graph");
    good(group.retire()); good(group.retire()); input.set(3);
    check(member_value == 1 && (*derived)->get() == 6 && other_value == 3,
          "Window group retirement suppressed derived state or another window");
    auto late = std::make_shared<lvui::native::Bindings>(); int late_value = 0;
    check(bool(late->bind([&](lvui::native::ReadContext& context) { return context.read(input); },
                          [&](int value) -> ii::core::Result<void> { late_value = value; return {}; })), "Cannot create late effect");
    good(group.track(late)); input.set(4);
    check(late_value == 3 && other_value == 4, "A graph admitted after window retirement kept its effects");
    bool wrong_thread = false;
    std::thread worker([&] { auto result = group.retire(); wrong_thread = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); check(wrong_thread, "Another thread retired a window group");
}
}

int main() { delivery_and_cancellation(); nested_close_and_failures(); owner_thread(); scoped_effect_groups(); }
