#include "NativeOwnedAlertButton.generated.h"
#include <iiGUI/Motion.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
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
void reactive_motion() {
    using namespace std::chrono_literals;using namespace ii::gui;using View=lvui_generated::NativeOwnedAlertButton;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);
    auto preferences=std::make_shared<MotionPreferences>();detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};hooks.preferences=preferences;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"motion-host",{300,200}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    const auto nodes=take(tree->snapshots());const auto found=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("/alert");});
    require(found!=nodes.end(),"Generated alert motion button did not mount");
    auto button=take(tree->find(found->id));(void)take(tree->drawing_snapshot({300,200}));const auto bounds=take(button.snapshot()).bounds;
    const auto alpha=[&]{
        const auto frame=take(tree->drawing_snapshot({300,200}));double value=0;
        const auto read=[&](const auto& panels){for(const auto& panel:panels)if(panel.bounds==bounds&&panel.stroke_width==0)value=panel.top.alpha;};
        read(frame.panels);for(const auto& layer:frame.overlays)read(layer.panels);return value;
    };
    require(std::abs(alpha())<1e-4,"Dialog button did not begin transparent");
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    const auto anchor=ii::runtime::Clock::now()+1h;(void)frames.step(anchor);
    if(std::abs(alpha()-.05)>=1e-4){const auto interaction=take(button.interaction());
        throw std::runtime_error("Generated alert did not reach its hover color: alpha="+std::to_string(alpha())+
            ", hovered="+std::to_string(interaction.hovered)+", bounds="+std::to_string(bounds.x)+","+std::to_string(bounds.y)+","+
            std::to_string(bounds.width)+","+std::to_string(bounds.height));}
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=299,.y=199}));(void)frames.step(anchor+65ms);
    require(alpha()>0&&alpha()<.05,"Generated alert did not animate its exit");
    scope->backend_0.motion_enabled.set(false);
    require(std::abs(alpha())<1e-4,"Reactive AlertButton motionEnabled did not settle the transition");
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    require(std::abs(alpha()-.05)<1e-4,"AlertButton did not retain its local motion opt-out");
    scope->backend_0.motion_enabled.set(true);
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=299,.y=199}));
    require(std::abs(alpha()-.05)<1e-4,"Generated alert did not resume local motion");
    (void)frames.step(anchor+130ms);require(alpha()>0&&alpha()<.05,"Generated alert did not resume animation");
    good(host.clear_content());scope.reset();tree->close();
}
}

int main() {
    reactive_motion();
    using View = lvui_generated::NativeOwnedAlertButton;
    using namespace ii::gui;
    auto tree = std::make_shared<detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({}, "host", {300, 200}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder) { return View::build(builder, {}, [](auto&) {}, &scope); }));
    const auto nodes = take(tree->snapshots());
    const auto found = std::ranges::find_if(nodes, [](const auto& item) { return item.id.ends_with("/alert"); });
    require(found != nodes.end(), "Generated AlertButton did not mount");
    auto button = take(tree->find(found->id));
    (void)take(tree->drawing_snapshot({300, 200}));
    auto snapshot = take(button.snapshot());
    require(!take(button.action_options()).enter_key_activation,"Generated AlertButton enabled Enter by default");
    good(button.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(scope->backend_0.events.empty(),"Default AlertButton accepted Enter");
    scope->backend_0.enter_enabled.set(true);
    require(take(button.action_options()).enter_key_activation,"AlertButton did not update its Enter policy");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    scope->backend_0.enter_enabled.set(false);
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(scope->backend_0.events==std::vector<std::string>{"pressed","canceled"},"AlertButton did not cancel Enter after policy change");
    scope->backend_0.events.clear();
    const auto x = snapshot.bounds.x + snapshot.bounds.width / 2, y = snapshot.bounds.y + snapshot.bounds.height / 2;
    good(tree->input({.kind = WidgetInputKind::pointer_down, .x = x, .y = y}));
    good(tree->input({.kind = WidgetInputKind::pointer_up, .x = x, .y = y}));
    require(scope->backend_0.events == std::vector<std::string>{"pressed", "released", "method", "after", "last", "clicked"} &&
            scope->backend_0.source_id == snapshot.id, "Generated AlertButton callbacks or event source failed");
    scope->backend_0.text.set("Save"); scope->backend_0.tone.set(lvui::ButtonTone::primary);
    require(take(button.snapshot()).text == "Save", "Generated AlertButton did not update reactively");
    auto graph = scope->native_lifetime.lock(); require(bool(graph), "Generated AlertButton bindings are unavailable");
    scope->backend_0.motion_enabled.set(false);
    require(take(graph->failures()).empty() && take(button.snapshot()).text == "Save",
            "Generated AlertButton did not accept a reactive local motion setting");
    scope->backend_0.motion_enabled.set(true);
    scope->backend_0.height.set(0);
    scope->backend_0.text.set("Retry");
    const auto failures = take(graph->failures());
    const auto rejected = take(button.snapshot());
    if (failures.empty() || rejected.bounds.height != 56 || rejected.text != "Save") {
        throw std::runtime_error("An invalid AlertButton input replaced its current state: failures=" +
            std::to_string(failures.size()) + ", height=" + std::to_string(rejected.bounds.height));
    }
    scope->backend_0.height.set(40);
    require(take(graph->failures()).empty() && take(button.snapshot()).bounds.height == 40 &&
            take(button.snapshot()).text == "Retry",
            "Generated AlertButton did not recover its valid height");
    std::weak_ptr<View::Scope> weak = scope; good(host.clear_content()); scope.reset(); graph.reset();
    require(weak.expired() && !button.snapshot(), "Generated AlertButton retained its activation"); tree->close();
}
