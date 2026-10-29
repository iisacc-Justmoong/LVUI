#include "NativeOwnedToggleSwitch.generated.h"
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
bool near(double a,double b){return std::abs(a-b)<1e-4;}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    const auto snapshots=take(state->snapshots());
    const auto found=std::ranges::find_if(snapshots,[&](const auto& item){return item.id.ends_with(suffix);});
    if(found==snapshots.end())throw std::runtime_error("Switch did not mount");return take(state->find(found->id));
}
double knob_x(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    const auto find_in=[&](const auto& panels)->std::optional<double>{
        for(const auto& panel:panels)if(near(panel.bounds.width,18)&&near(panel.bounds.height,18)&&near(panel.radius,9)&&
            near(panel.bounds.y,bounds.y+2))return panel.bounds.x-bounds.x;
        return std::nullopt;
    };
    if(auto found=find_in(frame.panels))return *found;
    for(const auto& layer:frame.overlays)if(auto found=find_in(layer.panels))return *found;
    throw std::runtime_error("Switch knob was not painted");
}
ii::gui::Color track_color(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    const auto find_in=[&](const auto& panels)->std::optional<ii::gui::Color>{
        for(const auto& panel:panels)if(near(panel.bounds.x,bounds.x)&&near(panel.bounds.y,bounds.y)&&
            near(panel.bounds.width,38)&&near(panel.bounds.height,22)&&near(panel.stroke_width,0))return panel.top;
        return std::nullopt;
    };
    if(auto found=find_in(frame.panels))return *found;
    for(const auto& layer:frame.overlays)if(auto found=find_in(layer.panels))return *found;
    throw std::runtime_error("Switch track was not painted");
}
void interaction(){
    using View=lvui_generated::NativeOwnedToggleSwitch;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{640,480}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));
    auto toggle=find(state,"/switch");auto initial=take(toggle.snapshot());
    require(initial.kind==ii::gui::WidgetKind::action_surface&&initial.action_role==ii::gui::ActionRole::toggle_switch&&
            initial.accessibility_name=="Snap to grid"&&!initial.checked&&near(initial.bounds.height,22),
            "Switch host semantics or initial state is wrong");
    auto pointer=[&](ii::gui::WidgetInputKind kind,double x,double y){good(state->input({.kind=kind,.x=x,.y=y}));};
    const auto x=initial.bounds.x+4,y=initial.bounds.y+11;
    pointer(ii::gui::WidgetInputKind::pointer_down,x,y);
    require(scope->backend_0.events.empty(),"Pointer press activated the switch before release");
    pointer(ii::gui::WidgetInputKind::pointer_up,x,y);
    require(take(toggle.snapshot()).checked&&scope->backend_0.events==std::vector<bool>{true},"Pointer activation lost state or callback");
    pointer(ii::gui::WidgetInputKind::pointer_move,initial.bounds.x-20,initial.bounds.y-20);
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Switch binding graph unavailable");
    scope->backend_0.on_color.set({.5,.25,.75,.75});
    const auto color=track_color(take(state->drawing_snapshot({640,480})),take(toggle.snapshot()).bounds);
    require(near(color.red,.21404114)&&near(color.green,.05087609)&&near(color.blue,.52252155)&&near(color.alpha,.75)&&
            take(toggle.snapshot()).checked,"Reactive color or linear-light conversion failed");
    scope->backend_0.on_color.set({2,0,0,1});
    require(!take(graph->failures()).empty()&&track_color(take(state->drawing_snapshot({640,480})),take(toggle.snapshot()).bounds)==color,
            "Invalid color changed the retained switch");
    scope->backend_0.on_color.set({.5,.25,.75,.75});require(take(graph->failures()).empty(),"Switch did not recover from an invalid color");
    scope->backend_0.text.set("Grid");
    require(take(toggle.snapshot()).checked,"An unrelated reactive change reset the user's selection");
    scope->backend_0.checked.set(true);scope->backend_0.checked.set(false);
    require(!take(toggle.snapshot()).checked&&scope->backend_0.events.size()==1,"Programmatic state update emitted a user callback");
    good(toggle.focus());good(state->input({.kind=ii::gui::WidgetInputKind::key_down,.key=ii::gui::WidgetKey::space}));
    good(state->input({.kind=ii::gui::WidgetInputKind::key_up,.key=ii::gui::WidgetKey::space}));
    require(take(toggle.snapshot()).checked&&scope->backend_0.events==std::vector<bool>({true,true}),"Keyboard activation failed");
    scope->backend_0.enabled.set(false);good(toggle.activate());
    require(!take(toggle.snapshot()).enabled&&scope->backend_0.events.size()==2,"Disabled switch admitted activation");
    scope->backend_0.enabled.set(true);scope->backend_0.checked.set(true);scope->backend_0.checked.set(false);
    auto bounds=take(toggle.snapshot()).bounds;
    pointer(ii::gui::WidgetInputKind::pointer_down,bounds.x+3,bounds.y+11);
    pointer(ii::gui::WidgetInputKind::pointer_move,bounds.x+19,bounds.y+11);
    require(!take(toggle.snapshot()).checked&&near(knob_x(take(state->drawing_snapshot({640,480})),bounds),18)&&
            scope->backend_0.events.size()==2,"Drag did not preview without committing");
    pointer(ii::gui::WidgetInputKind::pointer_up,bounds.x+19,bounds.y+11);
    require(take(toggle.snapshot()).checked&&scope->backend_0.events==std::vector<bool>({true,true,true}),"Drag did not commit once");
    scope->backend_0.rtl.set(true);bounds=take(toggle.snapshot()).bounds;
    pointer(ii::gui::WidgetInputKind::pointer_down,bounds.x+3,bounds.y+11);
    pointer(ii::gui::WidgetInputKind::pointer_move,bounds.x+19,bounds.y+11);
    pointer(ii::gui::WidgetInputKind::pointer_up,bounds.x+19,bounds.y+11);
    require(!take(toggle.snapshot()).checked&&scope->backend_0.events.back()==false&&scope->backend_0.events.size()==4,
            "RTL drag did not mirror the committed selection");
    bounds=take(toggle.snapshot()).bounds;
    pointer(ii::gui::WidgetInputKind::pointer_down,bounds.x+19,bounds.y+11);
    pointer(ii::gui::WidgetInputKind::pointer_move,bounds.x+3,bounds.y+11);
    good(state->input({.kind=ii::gui::WidgetInputKind::focus_lost}));
    require(!take(toggle.snapshot()).checked&&scope->backend_0.events.size()==4,"Canceled drag changed selection");
    scope->backend_0.width.set(10);
    require(!take(graph->failures()).empty()&&!take(toggle.snapshot()).checked,"Invalid metrics changed the retained switch");
    scope->backend_0.width.set(38);require(take(graph->failures()).empty(),"Switch did not recover after invalid metrics");
    bounds=take(toggle.snapshot()).bounds;
    pointer(ii::gui::WidgetInputKind::pointer_down,bounds.x+1,bounds.y+11);
    pointer(ii::gui::WidgetInputKind::pointer_up,bounds.x-1,bounds.y+11);
    require(!take(toggle.snapshot()).checked&&scope->backend_0.events.size()==4,"Outside release activated a non-dragged switch");
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=bounds.x+4,.y=bounds.y+11,.device=ii::gui::PointerDevice::touch}));
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=bounds.x+4,.y=bounds.y+11,.device=ii::gui::PointerDevice::touch}));
    require(take(toggle.snapshot()).checked&&scope->backend_0.events.size()==5,"Touch activation did not commit once");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!toggle.snapshot(),"Retired switch retained its scope");state->close();
}
void animation(){
    using namespace std::chrono_literals;using View=lvui_generated::NativeOwnedToggleSwitch;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},std::move(hooks));
    auto host=take(state->content_host({},"host",{640,480}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));
    auto toggle=find(state,"/switch");const auto bounds=take(toggle.snapshot()).bounds;
    scope->backend_0.motion_enabled.set(true);scope->backend_0.checked.set(true);
    const auto anchor=ii::runtime::Clock::now()+1h;(void)frames.step(anchor);
    require(near(knob_x(take(state->drawing_snapshot({640,480})),bounds),18),"Switch did not settle on");
    scope->backend_0.checked.set(false);(void)frames.step(anchor+160ms);
    require(near(knob_x(take(state->drawing_snapshot({640,480})),bounds),10)&&!take(toggle.snapshot()).checked,
            "Switch motion or immediate accessibility target is wrong");
    scope->backend_0.checked.set(true);(void)frames.step(anchor+320ms);
    require(near(knob_x(take(state->drawing_snapshot({640,480})),bounds),14),"Switch motion retarget did not preserve its current position");
    (void)frames.step(anchor+480ms);
    require(near(knob_x(take(state->drawing_snapshot({640,480})),bounds),18),"Switch motion did not finish on time");
    good(host.clear_content());scope.reset();state->close();
}
void retirement(){
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{320,200}));ii::gui::Widget toggle;
    std::optional<lvui::native::NativeToggleSwitch> component;int callbacks=0;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets)->ii::core::Result<void>{
        lvui::ToggleSwitchConfiguration config;config.motion_enabled=false;
        auto item=lvui::native::NativeToggleSwitch::create(widgets,{},"switch",config,[&](bool){++callbacks;good(host.clear_content());});
        if(!item)return std::unexpected(item.error());toggle=item->root();component=*item;return {};
    }));
    bool rejected=false;
    std::thread worker([&]{lvui::ToggleSwitchConfiguration value;value.checked=true;const auto result=component->set_options(value);
        rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected&&!take(toggle.snapshot()).checked,"Foreign thread changed switch state");
    good(toggle.activate());require(callbacks==1&&!toggle.snapshot(),"Toggle callback could not retire its generation safely");
    auto closed=component->set_options(lvui::ToggleSwitchConfiguration{});
    require(!closed&&closed.error().code==ii::core::ErrorCode::closed,"Retired switch remained writable");
    component.reset();state->close();
}
}
int main(){interaction();animation();retirement();}
