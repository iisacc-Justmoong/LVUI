#include "LVUINativeQuitOnlyFixture.generated.h"
#include <stdexcept>

namespace {
using View = lvui_generated::LVUINativeQuitOnlyFixture;
using Commands = lvui::native::ApplicationCommands;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void good(ii::core::Result<void> value) { if (!value) throw std::runtime_error(value.error().message); }
}
int main() {
    ii::runtime::Application app({}, {.workers={.workers=1}});
    auto owner = View::make_application_owner();
    good(owner->bind_close_loop(app.event_loop()));
    int calls = 0, failures = 0;
    auto commands = std::make_shared<Commands>([sender = app.event_loop().sender()](auto task) -> ii::core::Result<void> {
        if (!sender.post(std::move(task))) return ii::core::fail(ii::core::ErrorCode::closed, "Event loop rejected command");
        return {};
    }, [&]() -> ii::core::Result<void> { ++calls; return {}; }, [&](const auto&) { ++failures; });
    good(owner->bind_application_commands(commands));
    good(app.attach_before_driver(View::application_attachment("application", owner)));
    good(app.attach_before_driver({"application-commands", [](auto&) -> ii::core::Result<void> { return {}; },
                                   [commands]() -> ii::core::Result<void> { return commands->cancel(); }}));
    good(app.set_driver({[&](const auto& start) -> ii::core::Result<void> {
        check(calls == 0, "Startup quit ran before the driver");
        if (auto begun = start(); !begun) return begun;
        app.event_loop().poll();
        check(calls == 1 && failures == 0, "Startup quit did not run on a later event-loop turn");
        return {};
    }, []() -> ii::core::Result<void> { return {}; }}));
    check(bool(app.exec()) && !commands->quit(), "Startup quit or command retirement failed");
}
