#include "NativeOwnedProgress.generated.h"
#include "NativeOwnedProgressAdvanced.generated.h"
#include "NativeOwnedProgressSource.generated.h"
#include "NativeOwnedProgressScoped.generated.h"
#include "NativeOwnedProgressMotion.generated.h"
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template <class T> T take(ii::core::Result<T> result) {
    if (!result) throw std::runtime_error(result.error().message);
    return std::move(*result);
}
void good(ii::core::Result<void> result) { if (!result) throw std::runtime_error(result.error().message); }
bool near(double a, double b) { return std::abs(a - b) < 1e-4; }
std::vector<float> fill_widths(const ii::gui::CanvasFrame& drawing) {
    std::vector<float> widths;
    const auto collect = [&](const auto& panels) {
        for (const auto& panel : panels)
            if (near(panel.top.red, 0.007349484371406652) && near(panel.top.green, 0.22339124573309738) &&
                near(panel.top.blue, 1) && near(panel.top.alpha, 1) && panel.stroke_width == 0)
                widths.push_back(panel.bounds.width);
    };
    collect(drawing.panels);
    for (const auto& layer : drawing.overlays) collect(layer.panels);
    std::ranges::sort(widths);
    return widths;
}
std::vector<ii::gui::Panel> colored_panels(const ii::gui::CanvasFrame& drawing, float red, float green, float blue) {
    std::vector<ii::gui::Panel> matches;
    const auto collect = [&](const auto& panels) {
        for (const auto& panel : panels)
            if (near(panel.top.red, red) && near(panel.top.green, green) && near(panel.top.blue, blue) &&
                near(panel.top.alpha, 1) && panel.stroke_width == 0)
                matches.push_back(panel);
    };
    collect(drawing.panels);
    for (const auto& layer : drawing.overlays) collect(layer.panels);
    return matches;
}
void advanced() {
    using View = lvui_generated::NativeOwnedProgressAdvanced;
    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "host", {640, 480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
    auto snapshots = take(state->snapshots());
    auto found = std::ranges::find_if(snapshots, [](const auto& item) { return item.id.ends_with("/advanced"); });
    require(found != snapshots.end(), "Advanced progress did not mount");
    auto widget = take(state->find(found->id));
    const auto before = take(widget.snapshot());
    require(before.accessibility_semantics &&
            before.accessibility_semantics->role == ii::gui::AccessibilityRole::progress &&
            before.accessibility_name == "Progress" && near(before.value, .8) &&
            near(before.minimum, 0) && near(before.maximum, 1),
            "Progress semantics did not expose the initial normalized value");
    auto panels = colored_panels(take(state->drawing_snapshot({640, 480})), 0, 1, 0);
    require(panels.size() == 1 && near(panels[0].bounds.width, 100) && near(panels[0].radius, 1),
            "Advanced progress did not apply its initial style");
    auto graph = scope->native_lifetime.lock();
    require(bool(graph), "Advanced progress graph is unavailable");
    good(graph->begin_batch());
    scope->backend_0.width.set(150);
    scope->backend_0.height.set(10);
    scope->backend_0.displayed_start.set(.1);
    scope->backend_0.displayed_progress.set(.4);
    scope->backend_0.fill.set({1, 0, 0, 1});
    good(graph->end_batch());
    panels = colored_panels(take(state->drawing_snapshot({640, 480})), 1, 0, 0);
    const auto changed = take(widget.snapshot());
    require(panels.size() == 1 && near(panels[0].bounds.width, 60) &&
            near(changed.bounds.width, 150) && near(changed.bounds.height, 10) && before.id == changed.id,
            "A batch did not apply the retained progress configuration");
    scope->backend_0.width.set(-1);
    require(!take(graph->failures()).empty(), "An invalid progress configuration was accepted");
    require(near(take(widget.snapshot()).bounds.width, 150), "An invalid update changed the previous extent");
    scope->backend_0.width.set(175);
    require(take(graph->failures()).empty(), "A valid progress configuration did not recover");
    panels = colored_panels(take(state->drawing_snapshot({640, 480})), 1, 0, 0);
    require(panels.size() == 1 && near(panels[0].bounds.width, 70), "Recovered progress geometry is incorrect");
    good(graph->begin_batch());
    scope->backend_0.minimum.set(100);
    scope->backend_0.maximum.set(0);
    scope->backend_0.start.set(80);
    scope->backend_0.current.set(80);
    good(graph->end_batch());
    require(near(take(widget.snapshot()).value, .2), "Descending progress exposed an incorrect semantic value");
    good(host.clear_content());
    scope.reset();
    graph.reset();
    state->close();
}
void sourced() {
    using View = lvui_generated::NativeOwnedProgressSource;
    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "host", {640, 480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
    auto snapshots = take(state->snapshots());
    auto found = std::ranges::find_if(snapshots, [](const auto& item) { return item.id.ends_with("/sourced"); });
    require(found != snapshots.end(), "Sourced progress did not mount");
    auto widget = take(state->find(found->id));
    const auto before = take(widget.snapshot());
    const auto check_width = [&](float expected) {
        const auto widths = fill_widths(take(state->drawing_snapshot({640, 480})));
        if (widths.size() != 1 || !near(widths[0], expected)) {
            std::cerr << "Expected sourced fill width " << expected << ", got:";
            for (auto width : widths) std::cerr << ' ' << width;
            std::cerr << '\n';
        }
        require(widths.size() == 1 && near(widths[0], expected), "Sourced progress fill is incorrect");
    };
    check_width(80);
    auto graph = scope->native_lifetime.lock();
    require(bool(graph), "Sourced progress graph is unavailable");
    auto replacement = *scope->backend_0.state.get();
    replacement.numbers["alternate"] = 30;
    good(graph->begin_batch());
    scope->backend_0.state.set(replacement);
    scope->backend_0.key.set("alternate");
    good(graph->end_batch());
    check_width(40);
    require(take(widget.snapshot()).id == before.id, "A source update replaced the retained progress widget");
    scope->backend_0.state.set(std::nullopt);
    check_width(50);
    replacement.numbers["alternate"] = std::numeric_limits<double>::quiet_NaN();
    scope->backend_0.state.set(replacement);
    require(!take(graph->failures()).empty(), "A non-finite source value was accepted");
    check_width(50);
    replacement.numbers["alternate"] = 60;
    scope->backend_0.state.set(replacement);
    require(take(graph->failures()).empty(), "Sourced progress did not recover from a rejected value");
    check_width(100);
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content());
    scope.reset(); graph.reset();
    require(weak.expired() && !widget.snapshot(), "Sourced progress kept its scope or widget alive");
    state->close();
}
void scoped() {
    using View = lvui_generated::NativeOwnedProgressScoped;
    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "host", {640, 480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) {
        return View::build(builder, {}, [](View::Scope& setup) {
            setup.backend_0.numbers = {{"startValue", 10}, {"currentValue", 50}};
        }, &scope);
    }));
    const auto width = [&]() {
        const auto widths = fill_widths(take(state->drawing_snapshot({640, 480})));
        require(widths.size() == 1, "The scoped progress was not painted");
        return widths.front();
    };
    require(near(width(), 80), "The scoped progress ignored its source");
    scope->backend_0.numbers["currentValue"] = 70;
    scope->publish_native_changes();
    require(near(width(), 120), "The scoped source was not refreshed by its owner");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content());
    scope.reset();
    require(weak.expired(), "The scoped progress retained its source");
    state->close();
}
void motion() {
    using View = lvui_generated::NativeOwnedProgressMotion;
    using namespace std::chrono_literals;
    ii::runtime::EventLoop events;
    ii::runtime::FrameLoop frames(events);
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames = [&]() -> ii::core::Result<ii::runtime::FrameLoop*> { return &frames; };
    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); },
                                                               ii::gui::WidgetAppearance{}, std::move(hooks));
    auto host = take(state->content_host({}, "host", {640, 480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
    const auto width = [&]() {
        const auto widths = fill_widths(take(state->drawing_snapshot({640, 480})));
        require(widths.size() == 1, "Animated progress did not paint its fill");
        return widths.front();
    };
    auto graph = scope->native_lifetime.lock();
    require(bool(graph), "Animated progress graph is unavailable");
    auto snapshots = take(state->snapshots());
    auto found = std::ranges::find_if(snapshots, [](const auto& item) { return item.id.ends_with("/motion"); });
    require(found != snapshots.end(), "Animated progress did not mount");
    auto widget = take(state->find(found->id));
    scope->backend_0.current.set(100);
    const auto anchor = ii::runtime::Clock::now() + 1h;
    (void)frames.step(anchor);
    require(near(width(), 200), "Progress did not settle at its first animation target");
    scope->backend_0.enabled.set(false);
    scope->backend_0.current.set(0);
    scope->backend_0.enabled.set(true);
    scope->backend_0.current.set(100);
    (void)frames.step(anchor + 100ms);
    require(near(width(), 50), "Progress did not interpolate on the injected frame");
    require(near(take(widget.snapshot()).value, 1), "Accessibility followed an animation frame instead of the target");
    scope->backend_0.current.set(20);
    require(near(width(), 50), "Retargeting progress changed the displayed value");
    require(near(take(widget.snapshot()).value, .2), "Accessibility did not follow the retargeted value");
    (void)frames.step(anchor + 300ms);
    require(near(width(), 45), "Progress retarget did not continue from its displayed value");
    auto policy = scope->backend_0.motion.get();
    policy.reduced_motion = true;
    scope->backend_0.motion.set(policy);
    require(near(width(), 40), "Reduced motion did not settle at the new target");
    auto invalid = policy;
    invalid.duration = -1ms;
    scope->backend_0.motion.set(invalid);
    require(!take(graph->failures()).empty() && near(width(), 40), "Invalid motion changed the display");
    scope->backend_0.motion.set(policy);
    require(take(graph->failures()).empty(), "Progress did not recover from invalid motion");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content());
    scope.reset(); graph.reset();
    require(weak.expired(), "Animated progress retained its scope");
    state->close();
}
void missing_frame_source() {
    using View = lvui_generated::NativeOwnedProgressMotion;
    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "host", {640, 480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
    auto graph = scope->native_lifetime.lock();
    scope->backend_0.current.set(100);
    const auto widths = fill_widths(take(state->drawing_snapshot({640, 480})));
    require(!take(graph->failures()).empty() && widths.size() == 1 && near(widths[0], 200),
            "Missing frame source did not report failure and settle the target");
    scope->backend_0.enabled.set(false);
    require(take(graph->failures()).empty(), "Disabling motion did not recover frame admission");
    good(host.clear_content());
    scope.reset(); graph.reset();
    state->close();
}
}

int main() {
    using View = lvui_generated::NativeOwnedProgress;
    auto state = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(state->content_host({}, "host", {640, 480}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder) { return View::build(builder, {}, {}, &scope); }));
    auto snapshots = take(state->snapshots());
    auto dynamic = std::ranges::find_if(snapshots, [](const auto& item) { return item.id.ends_with("/dynamic"); });
    require(dynamic != snapshots.end(), "The progress view did not mount");
    auto widget = take(state->find(dynamic->id));
    const auto before = take(widget.snapshot());
    require(before.accessibility_semantics && near(before.value, .02),
            "Dynamic progress did not expose its initial numeric value");
    auto initial = fill_widths(take(state->drawing_snapshot({640, 480})));
    if (initial.size() != 2 || !near(initial[0], 18) || !near(initial[1], 60)) {
        std::cerr << "Initial fill widths:";
        for (auto width : initial) std::cerr << ' ' << width;
        std::cerr << "; dynamic bounds: " << before.bounds.width << 'x' << before.bounds.height << '\n';
    }
    require(initial.size() == 2 && near(initial[0], 18) && near(initial[1], 60),
            "The initial LVUI progress geometry was not drawn");
    scope->backend_0.count.set(80);
    auto changed = fill_widths(take(state->drawing_snapshot({640, 480})));
    require(changed.size() == 2 && near(changed[0], 60) && near(changed[1], 60),
            "The reactive value did not update the LVUI drawing");
    const auto after = take(widget.snapshot());
    require(before.id == after.id && before.bounds == after.bounds,
            "A value update replaced or resized the retained widget");
    require(after.accessibility_semantics && near(after.value, .8),
            "Progress semantics did not follow the reactive target");
    std::weak_ptr<View::Scope> weak = scope;
    good(host.clear_content());
    scope.reset();
    require(weak.expired() && !widget.snapshot(), "Retired progress kept its scope or widget alive");
    state->close();
    advanced();
    sourced();
    scoped();
    motion();
    missing_frame_source();
}
