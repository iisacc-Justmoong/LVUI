#include "NativeOwnedRadioButton.generated.h"
#include "NativeOwnedRadioRows.generated.h"
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
bool near(double a,double b){return std::abs(a-b)<1e-4;}
struct ReentrantPeer final:lvui::RadioGroupMember {
    bool selected_value=true,displayed=true,live=true;
    std::function<void()> during;
    std::string_view group()const noexcept override{return "choice";}
    bool selected()const noexcept override{return selected_value;}
    bool participating()const noexcept override{return live;}
    void assign(bool value)noexcept override{selected_value=value;}
    void publish()noexcept override{
        if(displayed==selected_value)return;displayed=selected_value;
        if(during){auto callback=during;callback();}
    }
};
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    const auto snapshots=take(state->snapshots());
    const auto found=std::ranges::find_if(snapshots,[&](const auto& item){return item.id.ends_with(suffix);});
    if(found==snapshots.end())throw std::runtime_error("Radio button did not mount");return take(state->find(found->id));
}
bool has(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    const auto snapshots=take(state->snapshots());return std::ranges::any_of(snapshots,[&](const auto& item){return item.id.ends_with(suffix);});
}
double dot_width(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    const auto scan=[&](const auto& panels)->std::optional<double>{
        for(const auto& panel:panels)if(near(panel.bounds.x+panel.bounds.width/2,bounds.x+9)&&
            near(panel.bounds.y+panel.bounds.height/2,bounds.y+bounds.height/2)&&panel.bounds.width<17&&panel.bounds.width>0&&near(panel.bounds.height,panel.bounds.width))return panel.bounds.width;
        return std::nullopt;
    };
    if(auto value=scan(frame.panels))return *value;
    for(const auto& layer:frame.overlays)if(auto value=scan(layer.panels))return *value;
    return 0;
}
void interaction(){
    using View=lvui_generated::NativeOwnedRadioButton;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{640,480}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));
    auto first=find(state,"/first"),other=find(state,"/other");auto snapshot=take(first.snapshot());
    require(snapshot.kind==ii::gui::WidgetKind::action_surface&&snapshot.action_role==ii::gui::ActionRole::radio_button&&
            snapshot.accessibility_name=="First choice"&&snapshot.checked&&near(snapshot.bounds.height,18)&&take(other.snapshot()).checked,
            "Initial radio semantics or group isolation is wrong");
    good(first.activate());require(scope->backend_0.events.empty()&&take(first.snapshot()).checked,"Selected radio toggled off");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Radio binding graph unavailable");
    scope->backend_0.candidate_size.set(0);scope->backend_0.show_child.set(true);
    require(!take(graph->failures()).empty()&&take(first.snapshot()).checked&&has(state,"/waiting")&&!has(state,"/second"),
            "Failed nested candidate changed the live group");
    scope->backend_0.candidate_size.set(17);scope->backend_0.show_child.set(false);scope->backend_0.show_child.set(true);
    require(take(graph->failures()).empty(),"Radio conditional did not recover");
    auto second=find(state,"/second");
    require(!take(first.snapshot()).checked&&take(second.snapshot()).checked&&take(other.snapshot()).checked&&scope->backend_0.events.empty(),
            "Committed nested selection did not reconcile its group");
    scope->backend_0.text.set("First updated");
    require(!take(first.snapshot()).checked&&take(second.snapshot()).checked,"Unrelated reactive input reset the group");
    good(first.focus());good(state->input({.kind=ii::gui::WidgetInputKind::key_down,.key=ii::gui::WidgetKey::space}));
    good(state->input({.kind=ii::gui::WidgetInputKind::key_up,.key=ii::gui::WidgetKey::space}));
    require(take(first.snapshot()).checked&&!take(second.snapshot()).checked&&scope->backend_0.events==std::vector<std::string>{"first:true"},
            "Keyboard selection or group callback is wrong");
    const auto b=take(second.snapshot()).bounds;
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=b.x+4,.y=b.y+9,.device=ii::gui::PointerDevice::touch}));
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=b.x+4,.y=b.y+9,.device=ii::gui::PointerDevice::touch}));
    require(!take(first.snapshot()).checked&&take(second.snapshot()).checked&&scope->backend_0.events==std::vector<std::string>({"first:true","second:true"}),
            "Touch selection or group callback is wrong");
    scope->backend_0.group.set("other");
    require(take(second.snapshot()).checked&&!take(first.snapshot()).checked&&!take(other.snapshot()).checked,
            "Reactive group move did not preserve one selection");
    scope->backend_0.on_color.set({2,0,0,1});
    require(!take(graph->failures()).empty()&&take(second.snapshot()).checked,"Invalid palette changed a live selection");
    scope->backend_0.on_color.set({.5,.25,.75,1});require(take(graph->failures()).empty(),"Radio palette did not recover");
    scope->backend_0.enabled.set(false);good(first.activate());
    require(!take(first.snapshot()).checked&&scope->backend_0.events.size()==2,"Disabled radio admitted activation");
    scope->backend_0.show_child.set(false);
    require(!second.snapshot()&&take(first.snapshot()).checked==false,"Retiring a selected child changed unrelated state");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!first.snapshot(),"Retired radio retained its scope");state->close();
}
void animation_and_retirement(){
    using namespace std::chrono_literals;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},std::move(hooks));
    auto host=take(state->content_host({},"host",{320,200}));auto groups=std::make_shared<lvui::RadioGroups>();
    std::optional<lvui::native::NativeRadioButton> radio;ii::gui::Widget widget;int callbacks=0;
    lvui::RadioButtonConfiguration config;config.checked=true;config.group="test";config.motion.curve=lvui::MotionCurve::linear;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto item=lvui::native::NativeRadioButton::create(builder,{},"radio",groups,config,[&](bool){++callbacks;good(host.clear_content());});
        if(!item)return std::unexpected(item.error());widget=item->root();radio=*item;return {};
    }));
    auto bounds=take(widget.snapshot()).bounds;config.checked=false;good(radio->set_options(config));
    const auto anchor=ii::runtime::Clock::now()+1h;(void)frames.step(anchor);
    require(near(dot_width(take(state->drawing_snapshot({320,200})),bounds),0),"Radio dot did not settle off");
    config.checked=true;good(radio->set_options(config));(void)frames.step(anchor+90ms);
    const auto halfway=dot_width(take(state->drawing_snapshot({320,200})),bounds);const auto selected=take(widget.snapshot()).checked;
    if(!near(halfway,4)||!selected)throw std::runtime_error("Radio half-frame width="+std::to_string(halfway)+" checked="+std::to_string(selected));
    (void)frames.step(anchor+180ms);
    require(near(dot_width(take(state->drawing_snapshot({320,200})),bounds),8),"Radio dot did not settle on time");
    bool rejected=false;std::thread worker([&]{auto result=radio->set_options(config);rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected&&take(widget.snapshot()).checked,"Foreign thread changed radio state");
    config.checked=false;good(radio->set_options(config));good(widget.activate());
    require(callbacks==1&&!widget.snapshot(),"Radio callback could not retire its generation safely");
    auto closed=radio->set_options(config);require(!closed&&closed.error().code==ii::core::ErrorCode::closed,"Retired radio remained writable");
    radio.reset();state->close();
}
void repeated_groups(){
    using View=lvui_generated::NativeOwnedRadioRows;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{640,480}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
    auto head=find(state,"/head"),list=find(state,"/rows");
    auto a=take(list.keyed_content_item("a","option")),b=take(list.keyed_content_item("b","option"));
    require(take(head.snapshot()).checked&&!take(a.snapshot()).checked&&!take(b.snapshot()).checked,"Initial repeated group is wrong");
    const auto original_id=take(a.snapshot()).id;auto graph=scope->native_lifetime.lock();
    auto values=scope->backend_0.items.get();values.push_back({"c","C",true,17});values.push_back({"bad","Bad",false,0});
    scope->backend_0.items.set(values);
    require(!take(graph->failures()).empty()&&take(head.snapshot()).checked&&take(a.snapshot()).id==original_id&&
            take(list.keyed_content_keys())==std::vector<std::string>({"a","b"}),"Failed repeated candidate changed the live group");
    values.back().box_size=17;scope->backend_0.items.set(values);
    require(take(graph->failures()).empty(),"Repeated group did not recover after a rejected candidate");
    auto c=take(list.keyed_content_item("c","option"));
    require(!take(head.snapshot()).checked&&take(c.snapshot()).checked&&take(a.snapshot()).id==original_id,
            "Published row did not select across generations");
    good(a.activate());require(take(a.snapshot()).checked&&!take(c.snapshot()).checked,"Row activation did not update its group");
    std::swap(values[0],values[2]);scope->backend_0.items.set(values);
    require(take(a.snapshot()).id==original_id&&take(a.snapshot()).checked&&!take(c.snapshot()).checked,
            "Reordering republished a retained row or changed selection");
    good(host.clear_content());scope.reset();graph.reset();require(!a.snapshot(),"Retired row remained active");state->close();
}
void reentrant_options(){
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{320,200}));auto groups=std::make_shared<lvui::RadioGroups>();
    auto peer=std::make_shared<ReentrantPeer>();require(groups->prepare(peer)&&groups->refresh(peer),"Peer registration failed");
    std::optional<lvui::native::NativeRadioButton> radio;ii::gui::Widget widget;
    lvui::RadioButtonConfiguration config;config.group="choice";config.text="Before";config.motion_enabled=false;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto item=lvui::native::NativeRadioButton::create(builder,{},"radio",groups,config);
        if(!item)return std::unexpected(item.error());widget=item->root();radio=*item;return {};
    }));
    peer->during=[&]{peer->during={};(void)groups->select(peer,true);};
    config.text="After";config.checked=true;good(radio->set_options(config));
    const auto snapshot=take(widget.snapshot());
    require(snapshot.text=="After"&&!snapshot.checked&&peer->selected_value,
            "Reentrant group selection lost unrelated radio options");
    good(host.clear_content());radio.reset();peer->live=false;groups->retire(peer);state->close();
}
}
int main(){interaction();animation_and_retirement();repeated_groups();reentrant_options();}
