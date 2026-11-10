#include "NativeOwnedTooltip.generated.h"
#include "NativeOwnedTooltipInline.generated.h"
#include "NativeOwnedTooltipReactive.generated.h"
#include "NativeOwnedTooltipDirect.generated.h"
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
ii::gui::Widget named(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view name){
    auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(name);});
    if(found==nodes.end())throw std::runtime_error("Generated Tooltip widget is missing");return take(tree->find(found->id));
}
void generated(){
    using View=lvui_generated::NativeOwnedTooltip;ii::runtime::EventLoop events;ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto outer=take(tree->content_host({},"outer",{640,420}));std::shared_ptr<View::Scope> scope;
    good(outer.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto anchor=named(tree,"/anchor"),tip=named(tree,"/tip"),label=named(tree,"/tip/text");auto bounds=take(anchor.snapshot()).bounds;
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    auto initial=take(tip.snapshot());require(initial.opened&&scope->backend_0.opened_count==1,"Generated tooltip did not open");
    scope->backend_0.text.set("A significantly longer help message");
    auto changed=take(tip.snapshot());auto text=take(label.snapshot()).text;
    auto graph=scope->native_lifetime.lock();auto failures=take(graph->failures());
    if(text!="A significantly longer help message"||!changed.opened||changed.size.width<=initial.size.width)
        throw std::runtime_error("Generated tooltip update: label="+text+" opened="+std::to_string(changed.opened)+
            " widths="+std::to_string(initial.size.width)+" -> "+std::to_string(changed.size.width)+
            " bounds="+std::to_string(initial.bounds.width)+" -> "+std::to_string(changed.bounds.width)+
            " failures="+std::to_string(failures.size()));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=600,.y=400}));
    require(!take(tip.snapshot()).opened&&scope->backend_0.closed_count==1,"Generated tooltip did not close");
    good(outer.clear_content());scope.reset();tree->close();
}
void inline_content(){
    using View=lvui_generated::NativeOwnedTooltipInline;ii::runtime::EventLoop events;ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto outer=take(tree->content_host({},"outer",{640,420}));std::shared_ptr<View::Scope> scope;
    good(outer.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto anchor=named(tree,"/anchor"),tip=named(tree,"/tip"),label=named(tree,"/help-label");auto bounds=take(anchor.snapshot()).bounds;
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    require(!take(tip.snapshot()).opened,"Delayed tooltip opened before its event-loop wake");
    const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(2);bool opened=false;
    do{(void)events.poll();opened=take(tip.snapshot()).opened;}while(!opened&&std::chrono::steady_clock::now()<limit);
    auto snapshot=take(tip.snapshot());
    require(opened&&snapshot.accessibility_name=="Help tooltip"&&take(label.snapshot()).text=="Help",
        "Inline tooltip content, scheduled opening or accessibility name was not displayed");
    scope->backend_0.text.set("A much longer inline tooltip message");
    (void)take(tree->drawing_snapshot({640,420}));auto changed=take(tip.snapshot());
    require(changed.opened&&changed.size.width>snapshot.size.width&&take(label.snapshot()).text=="A much longer inline tooltip message",
        "Reactive inline content did not resize the Tooltip");
    good(outer.clear_content());scope.reset();tree->close();
}
void reactive_options(){
    using View=lvui_generated::NativeOwnedTooltipReactive;ii::runtime::EventLoop events;ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto outer=take(tree->content_host({},"outer",{640,420}));std::shared_ptr<View::Scope> scope;
    good(outer.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto anchor=named(tree,"/reactive-anchor"),tip=named(tree,"/reactive-tip"),label=named(tree,"/reactive-tip/text");
    const auto anchor_bounds=take(anchor.snapshot()).bounds;const auto label_id=take(label.snapshot()).id;
    scope->backend_0.adjust_padding_on_open=true;
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=anchor_bounds.x+8,.y=anchor_bounds.y+8}));
    auto opened=take(tip.snapshot());auto graph=scope->native_lifetime.lock();
    require(opened.opened&&scope->backend_0.opened_count==1&&scope->backend_0.padding.get()==16&&
            take(graph->failures()).empty(),"A callback could not update reactive Tooltip options");
    if(opened.bounds.y<anchor_bounds.y+8)throw std::runtime_error("Initial Tooltip position y="+
        std::to_string(opened.bounds.y)+" h="+std::to_string(opened.bounds.height)+" anchor y="+std::to_string(anchor_bounds.y));
    scope->backend_0.placement.set(lvui::TooltipPlacement::above);
    auto above=take(tip.snapshot());
    require(above.opened&&above.bounds.y+above.bounds.height<=anchor_bounds.y+8.01&&
            take(label.snapshot()).id==label_id&&scope->backend_0.opened_count==1,
            "Reactive placement recreated or failed to reposition the Tooltip");
    scope->backend_0.surface.set(ii::gui::WidgetColor{1,0,0,1});
    auto drawing=take(tree->drawing_snapshot({640,420}));
    require(std::ranges::any_of(drawing.overlays,[](const auto& layer){return layer.backdrop&&layer.backdrop->mask&&
        layer.backdrop->tint.red>.99f&&layer.backdrop->tint.alpha>.49f;}),"Reactive Tooltip material did not update");
    scope->backend_0.name.set("Updated tooltip");
    require(take(tip.snapshot()).accessibility_name=="Updated tooltip","Reactive Tooltip accessibility name was not updated");
    scope->backend_0.timeout.set(60000);
    require(events.stats().pending==1,"Reactive timeout did not schedule a deadline");
    scope->backend_0.timeout.set(0);
    require(events.stats().pending==0&&take(tip.snapshot()).opened,"Disabling the timeout did not preserve the visible Tooltip");
    auto before_invalid=take(tip.snapshot());scope->backend_0.width.set(0);
    require(!take(graph->failures()).empty()&&take(tip.snapshot()).bounds==before_invalid.bounds&&
            take(label.snapshot()).id==label_id,"An invalid Tooltip width changed the active presentation");
    scope->backend_0.width.set(240);
    require(take(graph->failures()).empty()&&take(tip.snapshot()).opened,"Tooltip did not recover after a valid width");
    scope->backend_0.delay.set(1.5);
    require(!take(graph->failures()).empty()&&take(tip.snapshot()).opened,"A fractional Tooltip duration was accepted");
    scope->backend_0.delay.set(0);
    require(take(graph->failures()).empty(),"A valid Tooltip duration did not clear its binding failure");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=600,.y=400}));
    require(!take(tip.snapshot()).opened&&scope->backend_0.closed_count==1,"Reactive Tooltip did not close after leaving");
    scope->backend_0.automatic.set(false);
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=anchor_bounds.x+8,.y=anchor_bounds.y+8}));
    require(!take(tip.snapshot()).opened,"Disabled automatic Tooltip opened on hover");
    scope->backend_0.automatic.set(true);
    require(take(tip.snapshot()).opened&&scope->backend_0.opened_count==2&&take(label.snapshot()).id==label_id,
            "Reenabled automatic Tooltip did not open without replacing its content");
    std::weak_ptr<View::Scope> weak=scope;good(outer.clear_content());scope.reset();graph.reset();
    require(weak.expired(),"Tooltip option diagnostics retained a retired view scope");tree->close();
}
void deferred_option_failure(){
    using View=lvui_generated::NativeOwnedTooltipReactive;ii::runtime::EventLoop events(1);ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto outer=take(tree->content_host({},"outer",{640,420}));std::shared_ptr<View::Scope> scope;
    good(outer.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto anchor=named(tree,"/reactive-anchor"),tip=named(tree,"/reactive-tip");auto bounds=take(anchor.snapshot()).bounds;
    auto blocker=events.after(std::chrono::hours(1),[]{});require(bool(blocker),"Could not occupy the Tooltip scheduler");
    scope->backend_0.schedule_timeout_on_open=true;
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=bounds.x+8,.y=bounds.y+8}));
    const auto failures=take(scope->binding_failures());
    require(take(tip.snapshot()).opened&&std::ranges::any_of(failures,[](const auto& failure){
        return failure.failure.error.code==ii::core::ErrorCode::full;}),
        "A deferred Tooltip option failure was not available in view diagnostics");
    good(anchor.set_enabled(false));(void)take(anchor.snapshot());
    require(!take(tip.snapshot()).opened,"A failed deferred Tooltip option kept an unavailable target visible");
    (void)blocker.token.cancel();scope->backend_0.text.set("Retry after scheduler capacity returns");
    require(take(scope->binding_failures()).empty()&&!take(tip.snapshot()).opened,
        "A deferred Tooltip option did not retry after scheduler capacity returned");
    good(anchor.set_enabled(true));(void)take(anchor.snapshot());
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=bounds.x+8,.y=bounds.y+8}));
    require(take(tip.snapshot()).opened&&events.stats().pending==1,"The recovered Tooltip did not reopen with its timeout");
    scope->backend_0.timeout.set(0);
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=600,.y=400}));
    auto blocker2=events.after(std::chrono::hours(1),[]{});require(bool(blocker2),"Could not occupy the scheduler again");
    scope->backend_0.schedule_timeout_on_open=true;
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=bounds.x+8,.y=bounds.y+8}));
    require(!take(scope->binding_failures()).empty(),"A second deferred Tooltip failure was not recorded");
    scope->backend_0.timeout.set(0);(void)blocker2.token.cancel();
    scope->backend_0.text.set("A newer setting superseded the failed timeout");
    require(take(scope->binding_failures()).empty()&&events.stats().pending==0,
        "An older failed Tooltip option replaced a newer setting");
    scope->backend_0.timeout.set(60000);
    require(events.stats().pending==1,"Tooltip timeout did not recover after deferred failure");
    good(outer.clear_content());scope.reset();require(events.stats().pending==0,"Retired Tooltip kept a deferred timeout");tree->close();
}
void direct_target_binding(){
    using View=lvui_generated::NativeOwnedTooltipDirect;ii::runtime::EventLoop events;ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto outer=take(tree->content_host({},"outer",{640,420}));std::shared_ptr<View::Scope> scope;
    good(outer.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto first=named(tree,"/direct-first"),second=named(tree,"/direct-second"),tip=named(tree,"/direct-tip"),label=named(tree,"/direct-tip/text");
    const auto first_bounds=take(first.snapshot()).bounds,second_bounds=take(second.snapshot()).bounds;
    const auto label_id=take(label.snapshot()).id;
    scope->backend_0.direct_target.set(first);
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=first_bounds.x+8,.y=first_bounds.y+8}));
    require(take(tip.snapshot()).opened&&scope->backend_0.opened_count==1,"Typed Tooltip target did not open");
    scope->backend_0.bounds.set(ii::gui::Rect{0,0,640,100});auto bounded=take(tip.snapshot());
    require(bounded.opened&&bounded.bounds.y<100&&take(label.snapshot()).id==label_id&&take(scope->binding_failures()).empty(),
        "Typed available rectangle did not retain a target outside the body bounds");
    scope->backend_0.bounds.set(ii::gui::Rect{0,0,-1,100});
    require(!take(scope->binding_failures()).empty()&&take(tip.snapshot()).opened,
        "Invalid typed available rectangle replaced a live Tooltip");
    scope->backend_0.bounds.set(ii::gui::Rect{0,0,640,420});
    scope->backend_0.direct_target.set(second);
    require(!take(tip.snapshot()).opened&&scope->backend_0.closed_count==1&&take(scope->binding_failures()).empty(),
        "Typed target replacement did not close the old target's Tooltip");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=second_bounds.x+8,.y=second_bounds.y+8}));
    require(take(tip.snapshot()).opened&&scope->backend_0.opened_count==2,"Replacement typed target did not open");
    auto foreign_tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto foreign_host=take(foreign_tree->content_host({},"foreign-host",{100,100}));ii::gui::Widget foreign;
    good(foreign_host.set_content([&](auto& builder)->ii::core::Result<void>{auto node=builder.action_surface({},"foreign");
        if(!node)return std::unexpected(node.error());foreign=*node;return {};}));
    scope->backend_0.direct_target.set(foreign);
    require(!take(scope->binding_failures()).empty()&&take(tip.snapshot()).opened,
        "Foreign typed target replaced a live Tooltip target");
    scope->backend_0.direct_target.set(second);
    require(take(scope->binding_failures()).empty()&&take(tip.snapshot()).opened,"Typed target did not recover after rejection");
    scope->backend_0.direct_target.set(ii::gui::Widget{});require(!take(tip.snapshot()).opened,"Clearing typed target left Tooltip open");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=600,.y=400}));
    scope->backend_0.direct_target.set(first);scope->backend_0.next_target=second;scope->backend_0.switch_target_on_open=true;
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=first_bounds.x+8,.y=first_bounds.y+8}));
    require(!take(tip.snapshot()).opened&&scope->backend_0.direct_target.get()==second&&take(scope->binding_failures()).empty(),
        "Typed target update inside the open callback did not settle");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=second_bounds.x+8,.y=second_bounds.y+8}));
    require(take(tip.snapshot()).opened&&take(label.snapshot()).id==label_id,"Reentrant typed target lost Tooltip content");
    foreign_tree->close();std::weak_ptr<View::Scope> weak=scope;good(outer.clear_content());scope.reset();
    require(weak.expired(),"Typed Tooltip target binding retained a retired scope");tree->close();
}
void reactive_scrolling(){
    using View=lvui_generated::NativeOwnedTooltipReactive;ii::runtime::EventLoop events;ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto outer=take(tree->content_host({},"outer",{640,420}));std::shared_ptr<View::Scope> scope;
    good(outer.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    scope->backend_0.text.set(std::string(1000,'x'));
    auto anchor=named(tree,"/reactive-anchor"),tip=named(tree,"/reactive-tip");
    auto viewport=named(tree,"/reactive-tip/viewport"),label=named(tree,"/reactive-tip/text");
    const auto label_id=take(label.snapshot()).id;const auto bounds=take(anchor.snapshot()).bounds;
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=bounds.x+8,.y=bounds.y+8}));
    auto initial=take(viewport.snapshot());require(take(tip.snapshot()).opened&&initial.content_height>initial.bounds.height,
        "Reactive Tooltip did not expose long content for scrolling");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=initial.bounds.x+5,.y=initial.bounds.y+5}));
    require(take(tip.snapshot()).opened&&take(tip.interaction()).hovered,
        "Moving into a text-only Tooltip did not preserve its hover");
    good(tree->input({.kind=ii::gui::WidgetInputKind::scroll,.x=initial.bounds.x+5,.y=initial.bounds.y+5,.scroll_y=20}));
    require(take(viewport.snapshot()).scroll_offset==20&&take(tip.snapshot()).opened&&take(tip.interaction()).hovered,
        "Scrolling a Tooltip lost its content or bubble hover");
    auto graph=scope->native_lifetime.lock();const auto old_stats=take(graph->stats());
    const auto observers=scope->backend_0.scroll.stats().observers;
    scope->backend_0.scroll.set(false);const auto new_stats=take(graph->stats());
    (void)take(tree->drawing_snapshot({640,420}));auto clipped=take(viewport.snapshot());
    auto failures=take(scope->binding_failures());
    if(clipped.scroll_offset!=0||clipped.content_height!=clipped.bounds.height||take(label.snapshot()).id!=label_id||!failures.empty())
        throw std::runtime_error("Reactive scroll disable: offset="+std::to_string(clipped.scroll_offset)+
            " content="+std::to_string(clipped.content_height)+" viewport="+std::to_string(clipped.bounds.height)+
            " failures="+std::to_string(failures.size())+" observers="+std::to_string(observers)+
            " evaluations="+std::to_string(old_stats.evaluations)+"->"+std::to_string(new_stats.evaluations)+
            " opened="+std::to_string(take(tip.snapshot()).opened)+(failures.empty()?"":" "+failures.front().failure.error.message));
    scope->backend_0.scroll.set(true);auto enabled=take(viewport.snapshot());
    require(enabled.content_height>enabled.bounds.height&&take(tip.snapshot()).opened&&take(label.snapshot()).id==label_id&&
        take(scope->binding_failures()).empty(),"Reactive scroll enabling did not retain the visible Tooltip");
    good(outer.clear_content());scope.reset();tree->close();
}
void reactive_motion(){
    using namespace std::chrono_literals;using View=lvui_generated::NativeOwnedTooltipReactive;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto outer=take(tree->content_host({},"outer",{640,420}));std::shared_ptr<View::Scope> scope;
    good(outer.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto anchor=named(tree,"/reactive-anchor"),tip=named(tree,"/reactive-tip");
    const auto bounds=take(anchor.snapshot()).bounds;
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=bounds.x+5,.y=bounds.y+5}));
    auto time=ii::runtime::Clock::now();(void)frames.step(time+1h);
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=600,.y=400}));
    (void)frames.step(time+2h);
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=bounds.x+5,.y=bounds.y+5}));
    (void)frames.step(time+2h+80ms);
    require(take(tip.snapshot()).transitioning,"Generated Tooltip did not start its motion");
    scope->backend_0.motion_enabled.set(false);
    require(!take(tip.snapshot()).transitioning&&take(tip.snapshot()).opened,
        "Reactive motion disabling did not settle the existing Tooltip");
    scope->backend_0.animation_duration.set(-1);
    require(!take(scope->binding_failures()).empty()&&take(tip.snapshot()).opened,
        "An invalid reactive animation duration changed the Tooltip");
    scope->backend_0.animation_duration.set(200);
    require(take(scope->binding_failures()).empty(),"A valid animation duration did not recover its binding");
    good(outer.clear_content());scope.reset();tree->close();
}
}
int main(){generated();inline_content();reactive_options();deferred_option_failure();direct_target_binding();reactive_scrolling();reactive_motion();}
