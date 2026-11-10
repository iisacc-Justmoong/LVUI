#include "NativeOwnedHelpButton.generated.h"
#include <iiGUI/Motion.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool has_text(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& run:frame.texts)if(run.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return true;
    return false;
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Help button was not found");return take(tree->find(found->id));
}
void reactive_motion(){
    using namespace std::chrono_literals;using namespace ii::gui;using View=lvui_generated::NativeOwnedHelpButton;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);
    auto preferences=std::make_shared<MotionPreferences>();detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};hooks.preferences=preferences;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"motion-host",{300,150}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto button=find(tree,"/custom");(void)take(tree->drawing_snapshot({300,150}));const auto bounds=take(button.snapshot()).bounds;
    const auto fill=[&]{
        const auto frame=take(tree->drawing_snapshot({300,150}));std::optional<Color> value;
        const auto read=[&](const auto& panels){for(const auto& panel:panels)if(panel.bounds==bounds&&panel.stroke_width==0)value=panel.top;};
        read(frame.panels);for(const auto& layer:frame.overlays)read(layer.panels);
        if(!value){std::string detail=" bounds="+std::to_string(bounds.x)+","+std::to_string(bounds.y)+","+
            std::to_string(bounds.width)+","+std::to_string(bounds.height);
            const auto describe=[&](const auto& panels){for(const auto& p:panels)detail+=" panel="+std::to_string(p.bounds.x)+","+
                std::to_string(p.bounds.y)+","+std::to_string(p.bounds.width)+","+std::to_string(p.bounds.height)+","+std::to_string(p.stroke_width);};
            describe(frame.panels);for(const auto& layer:frame.overlays)describe(layer.panels);
            throw std::runtime_error("Generated HelpButton background is missing:"+detail);}return *value;
    };
    const auto same=[](Color a,Color b){return std::abs(a.red-b.red)<1e-4&&std::abs(a.green-b.green)<1e-4&&
        std::abs(a.blue-b.blue)<1e-4&&std::abs(a.alpha-b.alpha)<1e-4;};
    const auto idle=fill();
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    const auto anchor=ii::runtime::Clock::now()+1h;(void)frames.step(anchor);const auto hover=fill();
    require(!same(idle,hover),"Generated HelpButton did not change its hover color");
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=299,.y=149}));(void)frames.step(anchor+65ms);
    require(!same(fill(),idle)&&!same(fill(),hover),"Generated HelpButton did not animate its exit");
    scope->backend_0.motion_enabled.set(false);
    require(same(fill(),idle),"Reactive HelpButton motionEnabled did not settle the transition");
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    require(same(fill(),hover),"HelpButton did not retain its local motion opt-out");
    scope->backend_0.motion_enabled.set(true);
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=299,.y=149}));
    require(same(fill(),hover),"Generated HelpButton did not resume local motion");
    (void)frames.step(anchor+130ms);require(!same(fill(),idle)&&!same(fill(),hover),"Generated HelpButton did not resume animation");
    good(host.clear_content());scope.reset();tree->close();
}
}
int main(){
    reactive_motion();
    using View=lvui_generated::NativeOwnedHelpButton;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,150}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto frame=take(tree->drawing_snapshot({300,150}));auto help=find(tree,"/help"),custom=find(tree,"/custom");
    auto snap=take(help.snapshot());require(snap.bounds.width==21&&snap.bounds.height==21&&snap.accessibility_name=="Help"&&has_text(frame,"?"),
        "Default HelpButton lost its design or accessible name");
    require(!take(help.action_options()).enter_key_activation&&!take(custom.action_options()).enter_key_activation,
            "Generated HelpButton enabled Enter by default");
    good(custom.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(scope->backend_0.events.empty(),"Default HelpButton accepted Enter");
    scope->backend_0.enter_enabled.set(true);
    require(take(custom.action_options()).enter_key_activation,"HelpButton did not update its Enter policy");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    scope->backend_0.enter_enabled.set(false);
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(scope->backend_0.events==std::vector<std::string>{"pressed","canceled"},"HelpButton did not cancel Enter after policy change");
    scope->backend_0.events.clear();
    good(help.activate());good(custom.activate());require(scope->backend_0.clicks==2,"HelpButton activation failed");
    scope->backend_0.events.clear();const auto bounds=take(custom.snapshot()).bounds;
    const auto x=bounds.x+bounds.width/2,y=bounds.y+bounds.height/2;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));
    require(scope->backend_0.events==std::vector<std::string>{"pressed","released","method","after","last","clicked"},
            "Generated HelpButton did not deliver pointer events");
    scope->backend_0.events.clear();
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y}));
    good(tree->input({.kind=WidgetInputKind::focus_lost}));
    require(scope->backend_0.events==std::vector<std::string>{"pressed","canceled"},
            "Generated HelpButton did not deliver cancellation");
    scope->backend_0.text.set("More");scope->backend_0.width.set(44);frame=take(tree->drawing_snapshot({300,150}));
    require(take(custom.snapshot()).bounds.width==44&&has_text(frame,"More"),"Reactive HelpButton inputs were not applied");
    auto graph=scope->native_lifetime.lock();
    scope->backend_0.motion_enabled.set(false);
    require(take(graph->failures()).empty()&&take(custom.snapshot()).bounds.width==44,
            "Reactive HelpButton motion setting changed its retained state");
    scope->backend_0.motion_enabled.set(true);scope->backend_0.width.set(0);
    require(!take(graph->failures()).empty()&&take(custom.snapshot()).bounds.width==44,"Invalid HelpButton width replaced the live state");
    scope->backend_0.width.set(32);require(take(graph->failures()).empty(),"HelpButton did not recover from invalid options");
    scope->backend_0.enabled.set(false);good(custom.activate());require(scope->backend_0.clicks==3,"Disabled HelpButton accepted activation");
    good(host.clear_content());scope.reset();graph.reset();require(!help.snapshot(),"Retired HelpButton remained active");tree->close();
}
