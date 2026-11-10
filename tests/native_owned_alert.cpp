#include <LVUI/NativeAlert.h>
#include <iiGUI/NativeAlertView.h>
#include <iiGUI/AlertPresentation.h>
#include <iiGUI/SvgImageCache.h>
#include <iiGUI/WidgetState.h>

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace lvui::native {
using AlertInput=AlertInputValue<ii::gui::RasterImage>;
using AlertButtonEvent=ButtonEventValue<ii::gui::Widget>;
using NativeAlertCallbacks=NativeAlertCallbacksValue<ii::gui::Widget>;
using NativeAlert=AlertBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::BasicAlertButton,
    ii::gui::BasicNativeAlertButton,ii::gui::BasicAlert,ii::gui::BasicNativeAlert,ii::gui::CachedSvgDecoder,
    ii::gui::Widget,ii::gui::AlertPresentation>;
}
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T>T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
struct ReentrantIcon final:lvui::IconSource {
    mutable std::function<void()> during;mutable unsigned calls=0;std::uint64_t version=1;
    std::shared_ptr<const lvui::IconAsset> asset=std::make_shared<const lvui::IconAsset>(lvui::IconAsset{
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 2 2'><rect width='2' height='2'/></svg>"});
    lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view)const override {
        ++calls;if(during){auto callback=during;during={};callback();}return asset;
    }
    std::uint64_t revision()const noexcept override{return version;}
};
void asset_lifecycle(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"reentrant-host",{800,600}));
    lvui::native::AlertInput original;original.configuration.show_icon=false;original.configuration.motion_enabled=false;
    std::optional<lvui::native::NativeAlert> alert;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeAlert::create(builder,{},"alert",original);
        if(!value)return std::unexpected(value.error());alert=*value;return {};
    }));
    auto source=std::make_shared<ReentrantIcon>();std::weak_ptr<ReentrantIcon> weak=source;
    auto pending=original;pending.configuration.show_icon=true;pending.icon_name="custom";pending.icon_source=source;
    source->during=[&]{good(alert->set_options(original));};
    good(alert->set_options(pending));require(take(alert->options())==original,"Reentrant original options did not cancel the older icon request");
    good(alert->set_options(pending));const auto calls=source->calls;++source->version;
    good(alert->set_options(pending));require(source->calls==calls+1,"Alert ignored a changed icon source revision");
    ++source->version;source->during=[&]{good(host.clear_content());alert.reset();};
    auto retiring=pending;retiring.icon_revision=1;auto* active=&*alert;good(active->set_options(retiring));
    pending.icon_source.reset();retiring.icon_source.reset();source.reset();
    require(weak.expired(),"Retired LVUI alert retained an in-flight icon source");tree->close();
}
std::shared_ptr<const ii::gui::RasterImage> image(const ii::gui::CanvasFrame& frame) {
    for (const auto& item : frame.images) if (item.image) return item.image;
    for (const auto& layer : frame.overlays) for (const auto& item : layer.images) if (item.image) return item.image;
    return {};
}
}

int main() {
    asset_lifecycle();
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
    auto metric_update=updated;metric_update.configuration.metrics.action_height=60;
    metric_update.configuration.metrics.tertiary_action_height=38;
    good(alert->set_options(metric_update));
    require(take(primary.snapshot()).bounds.height==60&&take(tertiary.snapshot()).bounds.height==38,
            "Alert host did not apply custom action heights");
    good(alert->set_options(updated));
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
