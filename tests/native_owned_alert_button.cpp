#include <LVUI/NativeAlertButton.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
}

int main() {
    using namespace ii::gui;
    std::optional<lvui::native::NativeAlertButton> button;
    bool reenter = false, retire_on_method = false;
    lvui::AlertButtonConfiguration config;
    config.text = "Cancel"; config.dialog_style = true; config.width = 100; config.height = 56;
    auto latest = config; latest.text = "Newest";
    detail::WidgetFrameHooks hooks;
    hooks.accessibility_changed = [&] { if (reenter) { reenter = false; good(button->set_options(latest)); } };
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); }, WidgetAppearance{}, hooks);
    auto host = take(tree->content_host({}, "host", {300, 200}));
    std::vector<std::string> events;
    std::string source_id;
    good(host.set_content([&](auto& builder) -> ii::core::Result<void> {
        auto stack = builder.vstack({}, "root", {.spacing = 0, .alignment = StackAlignment::leading});
        if (!stack) return std::unexpected(stack.error());
        lvui::native::NativeAlertButtonCallbacks callbacks;
        callbacks.method = [&](const lvui::native::AlertButtonEvent& event) {
            source_id = take(event.source.snapshot()).id;
            require(event.trigger == "clicked", "AlertButton method received an invalid trigger");
            events.push_back("method");
            if (retire_on_method) good(host.clear_content());
        };
        callbacks.clicked = [&] { events.push_back("clicked"); };
        callbacks.pressed = [&] { events.push_back("pressed"); };
        callbacks.released = [&] { events.push_back("released"); };
        callbacks.canceled = [&] { events.push_back("canceled"); };
        auto created = lvui::native::NativeAlertButton::create(builder, *stack, "alert-button", config, std::move(callbacks));
        if (!created) return std::unexpected(created.error()); button = *created; return {};
    }));
    auto root = button->root();
    const auto frame = take(tree->drawing_snapshot({300, 200}));
    auto snapshot = take(root.snapshot());
    bool border = false;
    const auto inspect = [&](const auto& panels) {
        for (const auto& panel : panels) border |= panel.bounds == snapshot.bounds && panel.stroke_width == 1 && panel.radius == 16;
    };
    inspect(frame.panels); for (const auto& layer : frame.overlays) inspect(layer.panels);
    if (!border || snapshot.accessibility_name != "Cancel") {
        throw std::runtime_error("AlertButton outline=" + std::to_string(border) +
            " accessible=" + snapshot.accessibility_name + " text=" + snapshot.text);
    }
    const auto x = snapshot.bounds.x + snapshot.bounds.width / 2, y = snapshot.bounds.y + snapshot.bounds.height / 2;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = x, .y = y}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = x, .y = y}));
    require(events == std::vector<std::string>{"pressed", "released", "method", "clicked"} && source_id == snapshot.id,
            "AlertButton pointer or method callback order changed");
    events.clear(); good(root.focus());
    good(tree->input({.kind = WidgetInputKind::key_down, .key = WidgetKey::enter}));
    good(tree->input({.kind = WidgetInputKind::key_up, .key = WidgetKey::enter}));
    require(events == std::vector<std::string>{"pressed", "released", "method", "clicked"},
            "AlertButton keyboard callback order changed");

    auto disabled = config; disabled.enabled = false; good(button->set_options(disabled));
    events.clear(); good(root.activate()); require(events.empty(), "Disabled AlertButton admitted an action");
    auto invalid = config; invalid.height = 0;
    require(!button->set_options(invalid) && !take(button->options()).enabled,
            "Invalid AlertButton options replaced the last valid state");
    auto older = config; older.text = "Older"; reenter = true;
    good(button->set_options(older));
    require(!reenter && take(button->options()).text == "Newest" && take(root.snapshot()).text == "Newest",
            "An older AlertButton update replaced a reentrant update");
    bool rejected = false;
    std::thread worker([&] { auto result = button->set_options(config); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "AlertButton accepted a foreign-thread update");
    events.clear(); retire_on_method = true; good(root.activate());
    require(events == std::vector<std::string>{"method"} && !root.snapshot() && !button->options(),
            "A retired AlertButton delivered a stale click");
    tree->close();
}
