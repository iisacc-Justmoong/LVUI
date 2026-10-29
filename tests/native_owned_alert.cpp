#include <LVUI/NativeAlert.h>
#include <iiGUI/WidgetState.h>

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
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
std::shared_ptr<const ii::gui::RasterImage> image(const ii::gui::CanvasFrame& frame) {
    for (const auto& item : frame.images) if (item.image) return item.image;
    for (const auto& layer : frame.overlays) for (const auto& item : layer.images) if (item.image) return item.image;
    return {};
}
}

int main() {
    using namespace ii::gui;
    std::optional<lvui::native::NativeAlert> alert;
    bool reenter = false;
    std::optional<lvui::native::AlertInput> reentrant_input;
    detail::WidgetFrameHooks hooks;
    hooks.accessibility_changed = [&] {
        if (reenter) { reenter = false; good(alert->set_options(*reentrant_input)); }
    };
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); }, WidgetAppearance{}, hooks);
    auto host = take(tree->content_host({}, "host", {800, 600}));
    lvui::native::AlertInput input;
    input.configuration.motion_enabled = false;
    input.configuration.title = "Notice";
    input.configuration.message = "Continue?";
    input.configuration.action_text = {"Apply", "Cancel", ""};
    std::vector<std::string> events;
    bool reopen_on_method = false, reopen_on_close = false, reopen_and_close = false;
    good(host.set_content([&](auto& builder) -> ii::core::Result<void> {
        auto stack = builder.vstack({}, "root"); if (!stack) return std::unexpected(stack.error());
        lvui::native::NativeAlertCallbacks callbacks;
        callbacks.methods[0] = [&](const lvui::native::AlertButtonEvent& event) {
            require(event.trigger == "clicked" && bool(event.source.snapshot()), "Invalid alert action event");
            events.emplace_back("method");
            if (reopen_on_method) { reopen_on_method = false; good(alert->root().close_popup()); good(alert->root().open_at(0, 0)); }
        };
        callbacks.clicked[0] = [&] { events.emplace_back("primary"); };
        callbacks.clicked[1] = [&] { events.emplace_back("secondary"); };
        callbacks.closed = [&] {
            events.emplace_back("closed");
            if (reopen_on_close) { reopen_on_close = false; good(alert->root().open_at(0, 0)); }
            if (reopen_and_close) { reopen_and_close = false; good(alert->root().open_at(0, 0)); good(alert->root().close_popup()); }
        };
        callbacks.canceled = [&] { events.emplace_back("canceled"); };
        auto created = lvui::native::NativeAlert::create(builder, *stack, "alert", input, std::move(callbacks));
        if (!created) return std::unexpected(created.error()); alert = *created; return {};
    }));
    auto root = alert->root();
    (void)take(tree->drawing_snapshot({800, 600}));
    good(root.open_at(0, 0));
    auto frame = take(root.snapshot());
    require(frame.modal && frame.dim && frame.bounds.width == 500, "Alert did not open as a centered modal");
    const auto two_action_icon = image(take(tree->drawing_snapshot({800, 600})));
    require(bool(two_action_icon), "Alert did not render its LVUI-owned default icon");
    auto primary = take(root.popup_content("alert.action-0"));
    auto secondary = take(root.popup_content("alert.action-1"));
    auto tertiary = take(root.popup_content("alert.action-2"));
    const auto old_primary = take(primary.snapshot()).id;
    const auto p = take(primary.snapshot()), s = take(secondary.snapshot());
    require(p.visible && s.visible && !take(tertiary.snapshot()).visible &&
            s.bounds.x < p.bounds.x && p.bounds.height == 56, "Two alert actions were not arranged correctly");
    good(primary.activate());
    require(events == std::vector<std::string>{"method", "primary"} && take(root.snapshot()).opened,
            "Alert action ordering or automatic dismissal changed");
    auto updated = input;
    updated.configuration.action_text[2] = "Later";
    updated.configuration.action_enabled[1] = false;
    updated.configuration.message = "A longer notice that should wrap when the host becomes narrow.";
    good(alert->set_options(updated));
    const auto three_action_icon = image(take(tree->drawing_snapshot({800, 600})));
    require(three_action_icon && three_action_icon != two_action_icon, "Alert did not update its icon for three actions");
    const auto p3 = take(primary.snapshot()), s3 = take(secondary.snapshot()), t3 = take(tertiary.snapshot());
    require(p3.id == old_primary && p3.bounds.y < s3.bounds.y && s3.bounds.y < t3.bounds.y &&
            p3.bounds.height == 56 && t3.bounds.height == 44 && !s3.enabled,
            "Alert update lost retained actions or the vertical layout");
    good(secondary.activate()); require(events.size() == 2, "Disabled alert action was delivered");
    auto invalid = updated; invalid.configuration.maximum_width = 0;
    require(!alert->set_options(invalid) && take(alert->options()).configuration == updated.configuration,
            "Invalid alert options replaced the current configuration");
    auto oversized = updated; oversized.configuration.content_bottom = 8192;
    require(!alert->set_options(oversized) && take(alert->options()).configuration == updated.configuration &&
            take(primary.snapshot()).text == "Apply" && !take(alert->layout_failure()),
            "Unrenderable alert geometry did not roll back its update");
    reentrant_input = updated; reentrant_input->configuration.title = "Newest";
    auto older = updated; older.configuration.title = "Older"; reenter = true;
    good(alert->set_options(older));
    require(!reenter && take(alert->options()).configuration.title == "Newest" &&
            take(take(root.popup_content("alert.title")).snapshot()).text == "Newest",
            "An older Alert update overwrote a reentrant update");
    updated = *reentrant_input;
    const auto wide = take(root.snapshot()).bounds;
    (void)take(tree->drawing_snapshot({300, 600}));
    const auto narrow = take(root.snapshot()).bounds;
    require(narrow.width == 252 && narrow.height > wide.height && !take(alert->layout_failure()),
            "Alert did not fit and remeasure text in the narrow viewport");
    (void)take(tree->drawing_snapshot({800, 600}));
    require(take(root.snapshot()).bounds.width == 500 && take(root.snapshot()).bounds.height == wide.height,
            "Alert did not restore its geometry when the viewport widened");
    good(root.close_popup()); good(root.open_at(0, 0));
    require(take(take(root.popup_content("alert.action-0")).snapshot()).id == old_primary,
            "Alert recreated static actions across openings");
    events.clear(); reopen_on_method = true; good(primary.activate());
    require(events == std::vector<std::string>{"method", "closed"} && take(root.snapshot()).opened,
            "A retired opening delivered its later click after a reentrant reopen");
    events.clear(); reopen_on_close = true; good(root.cancel_popup());
    require(events == std::vector<std::string>{"closed"} && take(root.snapshot()).opened,
            "A stale cancellation reached a reopened alert");
    good(root.cancel_popup()); require(events == std::vector<std::string>{"closed", "closed", "canceled"},
            "Alert cancellation did not notify exactly once");
    events.clear(); good(root.open_at(0, 0)); reopen_and_close = true; good(root.cancel_popup());
    require(events == std::vector<std::string>{"closed", "closed"} && !take(root.snapshot()).opened,
            "An old cancellation survived an immediate reopen and close");
    bool rejected = false;
    std::thread worker([&] { auto result = alert->set_options(input); rejected = !result && result.error().code == ii::core::ErrorCode::wrong_executor; });
    worker.join(); require(rejected, "Alert accepted a foreign-thread update");
    good(host.clear_content()); require(!alert->options() && !primary.snapshot(), "Retired alert retained its actions");
    tree->close();
}
