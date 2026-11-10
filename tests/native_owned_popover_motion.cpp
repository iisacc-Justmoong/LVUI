#include "native_popover_composition.h"
#include <LVUI/NativePopover.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>

#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

using namespace std::chrono_literals;

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class T> T take(ii::core::Result<T> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) {
        if (value.error().cause) std::rethrow_exception(value.error().cause);
        throw std::runtime_error(value.error().message);
    }
}
bool near(double value, double expected, double tolerance = .01) {
    return std::abs(value - expected) < tolerance;
}

struct Fixture final {
    ii::runtime::EventLoop events;
    ii::runtime::FrameLoop frames{events};
    std::shared_ptr<ii::gui::MotionPreferences> preferences = std::make_shared<ii::gui::MotionPreferences>();
    std::shared_ptr<ii::gui::detail::WidgetState> tree;
    ii::gui::Widget host, label;
    std::optional<lvui::native::NativePopover> popup;
    ii::runtime::Clock::time_point start;
    int opened = 0, closed = 0;

    explicit Fixture(bool with_frames = true, bool motion_enabled = true) {
        ii::gui::detail::WidgetFrameHooks hooks;
        if (with_frames) hooks.frames = [&]() -> ii::core::Result<ii::runtime::FrameLoop*> { return &frames; };
        hooks.preferences = preferences;
        tree = std::make_shared<ii::gui::detail::WidgetState>(
            [](ii::runtime::Task task) { task(); }, ii::gui::WidgetAppearance{}, std::move(hooks));
        host = take(tree->content_host({}, "host", {400, 300}));
        good(host.set_content([&](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
            auto root = take(builder.vstack({}, "root"));
            lvui::PopoverConfiguration config;
            config.width = 160;
            config.height = 100;
            config.motion_enabled = motion_enabled;
            popup = take(lvui::native::NativePopover::create(builder, root, "popover", config,
                {.opened = [&] { ++opened; }, .closed = [&] { ++closed; }}));
            label = take(builder.label(popup->root(), "label", {.text = "Motion content"}));
            return {};
        }));
        (void)take(tree->drawing_snapshot({400, 300}));
    }
    ~Fixture() { tree->close(); }
    void open() { good(popup->root().open_at(20, 20)); }
    void close() { good(popup->root().close_popup()); }
    void step(std::chrono::milliseconds delta) { (void)frames.step(start + delta); }
    ii::gui::ContentTransform transform() {
        const auto scene = take(label.snapshot_scene());
        for (const auto& layer : scene.layers)
            for (const auto& text : layer.texts)
                if (text.text == "Motion content") return layer.content_transform;
        throw std::runtime_error("Animated Popover content was not retained in its scene");
    }
    void reset_clock_and_open() {
        start = ii::runtime::Clock::now();
        open(); step(1h); close(); step(2h); open();
        start += 2h;
        opened = closed = 0;
    }
};

void independent_tracks_and_reopening() {
    Fixture f;
    f.reset_clock_and_open();
    f.step(80ms);
    const auto first = f.transform();
    const auto t = 80. / 420. - 1.;
    const auto scale = .92 + .08 * (1. + 2.45 * t * t * t + 1.45 * t * t);
    require(near(first.opacity, .5) && near(first.scale, scale),
        "Popover did not use the independent 160ms opacity and 420ms OutBack scale tracks");
    f.step(160ms);
    require(near(f.transform().opacity, 1), "Popover enter opacity did not finish at 160ms");
    f.step(420ms);
    require(near(f.transform().scale, 1), "Popover scale did not settle at 420ms");
    f.close();
    const auto snapshot = take(f.popup->root().snapshot());
    require(!snapshot.opened && snapshot.visible && snapshot.exiting && f.closed == 1,
        "Popover exit did not retain pixels after immediately closing input");
    f.step(495ms);
    const auto exiting = f.transform();
    require(near(exiting.opacity, .5) && near(exiting.scale, 1),
        "Popover exit did not hold its scale and fade over 150ms");
    f.open();
    const auto reopened = f.transform();
    require(near(reopened.opacity, exiting.opacity, .002) && near(reopened.scale, exiting.scale, .002),
        "Reopening interrupted the displayed Popover transform");
    f.step(575ms);
    require(near(f.transform().opacity, .75), "Reopening did not continue from the displayed opacity");
}

void local_and_global_motion() {
    Fixture f;
    f.reset_clock_and_open();
    f.step(80ms);
    auto options = take(f.popup->options());
    options.motion_enabled = false;
    good(f.popup->set_options(options));
    const auto settled = take(f.popup->root().snapshot());
    require(settled.opened && !settled.transitioning && near(f.transform().opacity, 1),
        "Disabling local Popover motion did not settle the current transition");
    options.motion_enabled = true;
    good(f.popup->set_options(options));
    good(f.preferences->set_reduced_motion(true));
    f.close();
    require(!take(f.popup->root().snapshot()).visible, "Reduced motion retained an exit frame");
    f.open();
    require(!take(f.popup->root().snapshot()).transitioning, "Reduced motion animated an open Popover");
    good(f.preferences->set_reduced_motion(false));
}

void shared_speed() {
    Fixture f;
    good(f.preferences->set_speed(2));
    f.reset_clock_and_open();
    f.step(80ms);
    const auto animated = f.transform();
    const auto t = 160. / 420. - 1.;
    const auto scale = .92 + .08 * (1. + 2.45 * t * t * t + 1.45 * t * t);
    require(near(animated.opacity, 1) && near(animated.scale, scale),
        "Popover ignored the shared motion speed");
    f.step(210ms);
    require(near(f.transform().scale, 1), "Popover scale did not settle at the shared speed");
}

void motion_setting_is_local() {
    ii::runtime::EventLoop events;
    ii::runtime::FrameLoop frames{events};
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames = [&]() -> ii::core::Result<ii::runtime::FrameLoop*> { return &frames; };
    auto tree = std::make_shared<ii::gui::detail::WidgetState>(
        [](ii::runtime::Task task) { task(); }, ii::gui::WidgetAppearance{}, std::move(hooks));
    auto host = take(tree->content_host({}, "host", {400, 300}));
    std::optional<lvui::native::NativePopover> first, second;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        auto root = take(builder.vstack({}, "root"));
        first = take(lvui::native::NativePopover::create(builder, root, "first"));
        second = take(lvui::native::NativePopover::create(builder, root, "second"));
        return {};
    }));
    (void)take(tree->drawing_snapshot({400, 300}));
    const auto start = ii::runtime::Clock::now();
    good(first->root().open_at(20, 20));good(second->root().open_at(40, 40));
    (void)frames.step(start + 80ms);
    auto config = take(first->options());config.motion_enabled = false;good(first->set_options(config));
    require(!take(first->root().snapshot()).transitioning && take(second->root().snapshot()).transitioning,
        "Disabling one Popover's motion changed another Popover");
    tree->close();
}

void no_frame_source() {
    Fixture f(false);
    f.open();
    require(take(f.popup->root().snapshot()).opened && !take(f.popup->root().snapshot()).transitioning,
        "A host without frames did not open its Popover immediately");
    auto options = take(f.popup->options());
    options.motion_enabled = false;
    good(f.popup->set_options(options));
    options.motion_enabled = true;
    good(f.popup->set_options(options));
    f.close();
    require(!take(f.popup->root().snapshot()).visible,
        "A host without frames retained an exit frame");

    Fixture initially_disabled(false, false);
    auto disabled = take(initially_disabled.popup->options());
    disabled.motion_enabled = true;
    good(initially_disabled.popup->set_options(disabled));
    initially_disabled.open();
    require(!take(initially_disabled.popup->root().snapshot()).transitioning,
        "Enabling motion on a frameless host did not retain the immediate fallback");
}

void frame_source_errors_are_reported() {
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames = []() -> ii::core::Result<ii::runtime::FrameLoop*> {
        return ii::core::fail(ii::core::ErrorCode::failed, "Frame source failed");
    };
    auto tree = std::make_shared<ii::gui::detail::WidgetState>(
        [](ii::runtime::Task task) { task(); }, ii::gui::WidgetAppearance{}, std::move(hooks));
    auto host = take(tree->content_host({}, "host", {400, 300}));
    auto result = host.set_content([](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        auto popup = lvui::native::NativePopover::create(builder, {}, "popup");
        if (!popup) return std::unexpected(popup.error());
        return {};
    });
    require(!result && result.error().code == ii::core::ErrorCode::failed,
        "A frame source error was silently converted into an unsupported-host fallback");
    std::optional<lvui::native::NativePopover> popup;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        lvui::PopoverConfiguration config;config.motion_enabled = false;
        popup = take(lvui::native::NativePopover::create(builder, {}, "still-popup", config));
        return {};
    }));
    auto options = take(popup->options());options.motion_enabled = true;
    auto failed = popup->set_options(options);
    require(!failed && failed.error().code == ii::core::ErrorCode::failed &&
            !take(popup->options()).motion_enabled,
        "Enabling motion did not report the frame failure and preserve the still Popover");
    good(popup->root().open_at(20, 20));
    require(take(popup->root().snapshot()).opened && !take(popup->root().snapshot()).transitioning,
        "An explicitly still Popover depended on a failed frame source");
    tree->close();
}

void unsupported_content_is_not_a_missing_frame_source() {
    ii::runtime::EventLoop events;
    ii::runtime::FrameLoop frames{events};
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames = [&]() -> ii::core::Result<ii::runtime::FrameLoop*> { return &frames; };
    auto tree = std::make_shared<ii::gui::detail::WidgetState>(
        [](ii::runtime::Task task) { task(); }, ii::gui::WidgetAppearance{}, std::move(hooks));
    auto host = take(tree->content_host({}, "host", {400, 300}));
    std::optional<lvui::native::NativePopover> popup;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        lvui::PopoverConfiguration config;
        config.motion_enabled = false;
        popup = take(lvui::native::NativePopover::create(builder, {}, "popup", config));
        return {};
    }));
    good(popup->root().set_popup_content([](ii::gui::WidgetBuilder& builder) -> ii::core::Result<void> {
        auto label = builder.label({}, "dynamic", {.text = "Dynamic"});
        if (!label) return std::unexpected(label.error());
        return {};
    }));
    auto next = take(popup->options());
    next.motion_enabled = true;
    auto result = popup->set_options(next);
    require(!result && result.error().code == ii::core::ErrorCode::unsupported &&
            !take(popup->options()).motion_enabled,
        "Unsupported animated activation content changed the Popover configuration");
    good(popup->root().open_at(20, 20));
    auto content = take(popup->root().popup_content("dynamic"));
    require(take(content.snapshot()).text == "Dynamic", "A rejected motion change lost the previous activation content");
    tree->close();
}
}

int main() {
    independent_tracks_and_reopening();
    local_and_global_motion();
    shared_speed();
    motion_setting_is_local();
    no_frame_source();
    frame_source_errors_are_reported();
    unsupported_content_is_not_a_missing_frame_source();
}
