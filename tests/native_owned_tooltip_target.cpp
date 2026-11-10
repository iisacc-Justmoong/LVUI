#include "tooltip_target_test_binding.h"
#include <LVUI/NativeTooltip.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace {
using namespace std::chrono_literals;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct OtherPort final:lvui::TooltipPresentationPort {
    ii::core::Result<void> present(std::optional<lvui::TooltipLayout>)override{return {};}
    ii::core::Result<void> set_deadline(std::optional<std::int64_t>)override{return {};}
};
void observe(){
    ii::runtime::EventLoop events;auto scheduler=take(TooltipTestScheduler::create(events.sender()));
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));ii::gui::Widget first,second,label_widget;int clicks=0;
    std::shared_ptr<TooltipTestPresentation> port;std::optional<TooltipTestController> controller;
    std::optional<TooltipTestTarget> target;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"layout",{640,480});if(!root)return std::unexpected(root.error());
        auto a=builder.action_surface(*root,"first",{.width=100,.height=30});if(!a)return std::unexpected(a.error());first=*a;
        if(auto frame=first.set_frame({200,200,100,30});!frame)return frame;
        auto b=builder.action_surface(*root,"second",{.width=100,.height=30},[&]{++clicks;});if(!b)return std::unexpected(b.error());second=*b;
        if(auto frame=second.set_frame({400,200,100,30});!frame)return frame;
        auto created=TooltipTestPresentation::create(builder,*root,"tooltip",{},scheduler.weak_schedule());
        if(!created)return std::unexpected(created.error());port=*created;
        auto label=builder.label(port->content_root(),"tooltip-label",{.text="Helpful text",.font_size=13,.selectable=false});
        if(!label)return std::unexpected(label.error());label_widget=*label;
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=0ms;config.hide_delay=0ms;
        auto owner=TooltipTestController::create(port,config);if(!owner)return std::unexpected(owner.error());controller=*owner;
        auto other_controller=TooltipTestController::create(std::make_shared<OtherPort>(),config);
        if(!other_controller)return std::unexpected(other_controller.error());
        auto mismatch=TooltipTestTarget::create(builder,port,*other_controller,scheduler);
        if(mismatch)return ii::core::fail(ii::core::ErrorCode::failed,"Mismatched tooltip port was accepted");
        auto observing=TooltipTestTarget::create(builder,port,*controller,scheduler);
        if(!observing)return std::unexpected(observing.error());target=*observing;
        if(auto wake=scheduler.set_wake(target->weak_tick());!wake)return wake;
        return target->set_target(first);
    }));
    (void)take(first.snapshot());good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=210,.y=210}));
    require(take(port->root().snapshot()).opened&&!take(target->last_failure()),"Target hover did not open the tooltip");
    const auto previous_width=take(port->root().snapshot()).size.width;
    good(label_widget.set_text("A substantially longer inline tooltip message"));
    (void)take(tree->drawing_snapshot({640,480}));
    require(take(port->root().snapshot()).size.width>previous_width&&!take(target->last_failure()),
        "Changed inline Tooltip content did not trigger a new measurement");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=20,.y=20}));
    require(!take(port->root().snapshot()).opened,"Leaving the target did not close the tooltip");
    good(first.focus());(void)take(first.snapshot());require(take(port->root().snapshot()).opened,"Target focus did not open the tooltip");
    good(first.set_enabled(false));(void)take(first.snapshot());require(!take(port->root().snapshot()).opened,"Disabled target left tooltip visible");
    good(target->set_target(second));good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=410,.y=210}));
    require(take(port->root().snapshot()).opened&&!take(target->last_failure()),"Replacement target did not open the tooltip");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=410,.y=210}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=410,.y=210}));
    require(clicks==1,"Tooltip observation replaced the target's input handler");
    auto other=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto other_host=take(other->content_host({},"other",{100,100}));ii::gui::Widget foreign;
    good(other_host.set_content([&](auto& builder)->ii::core::Result<void>{auto root=builder.absolute_stack({},"root",{100,100});
        if(!root)return std::unexpected(root.error());auto value=builder.action_surface(*root,"foreign");if(!value)return std::unexpected(value.error());foreign=*value;return {};}));
    auto rejected=target->set_target(foreign);require(!rejected&&rejected.error().code==ii::core::ErrorCode::invalid_argument&&
        take(port->root().snapshot()).opened,"A foreign target replaced the active target");other->close();
    good(target->set_target({}));require(!take(port->root().snapshot()).opened,"Clearing the target left its tooltip open");
    auto late=target->weak_tick();good(outer.clear_content());auto retired=late(0);
    require(!retired&&retired.error().code==ii::core::ErrorCode::closed,"Retired target accepted a scheduled wake");
    target.reset();controller.reset();port.reset();good(scheduler.retire());tree->close();
}
void facade(){
    require(lvui::native::tooltip_duration(0)==0ms&&lvui::native::tooltip_duration(86400000)==86400000ms&&
        lvui::native::tooltip_duration(.5)==-1ms&&lvui::native::tooltip_duration(86400001)==-1ms&&
        lvui::native::tooltip_duration(std::numeric_limits<double>::quiet_NaN())==-1ms&&
        lvui::native::tooltip_duration(std::numeric_limits<double>::infinity())==-1ms,
        "Tooltip duration conversion accepted an invalid value");
    ii::runtime::EventLoop events;ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto outer=take(tree->content_host({},"outer",{640,480}));ii::gui::Widget anchor;
    std::optional<TooltipTestFacade> tooltip;int opened=0,closed=0;bool callback_refresh=false,callback_options=false;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"layout",{640,480});if(!root)return std::unexpected(root.error());
        auto action=builder.action_surface(*root,"anchor",{.width=100,.height=30});if(!action)return std::unexpected(action.error());anchor=*action;
        if(auto framed=anchor.set_frame({200,200,100,30});!framed)return framed;
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=0ms;config.hide_delay=0ms;
        auto created=TooltipTestFacade::create(builder,*root,"tooltip",config,{}, {},
            {.opened=[&]{++opened;callback_refresh=bool(tooltip->refresh());
                if(opened==1){auto value=take(tooltip->options());value.configuration.content_padding=18;
                    value.style.content_padding=18;callback_options=bool(tooltip->set_options(std::move(value)));}},
             .closed=[&]{++closed;}});
        if(!created)return std::unexpected(created.error());tooltip=*created;
        auto diagnostics=take(tooltip->diagnostic_source());
        static_assert(std::is_const_v<typename decltype(diagnostics)::element_type>);
        auto candidate=take(tooltip->options());candidate.configuration.content_padding=14;candidate.style.content_padding=14;
        candidate.target.anchor=lvui::TooltipPoint{25,10};if(auto changed=tooltip->set_options(candidate);!changed)return changed;
        if(take(tooltip->options())!=candidate)return ii::core::fail(ii::core::ErrorCode::failed,"Staged Tooltip options were not retained");
        auto label=builder.label(tooltip->content_root(),"help",{.text="A helpful tooltip",.font_size=13,.selectable=false});
        if(!label)return std::unexpected(label.error());if(auto refreshed=tooltip->refresh();!refreshed)return refreshed;
        return tooltip->set_target(anchor);
    }));
    (void)take(anchor.snapshot());good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=210,.y=210}));
    require(take(tooltip->root().snapshot()).opened&&opened==1&&callback_refresh&&callback_options&&
        take(tooltip->options()).configuration.content_padding==18&&!take(tooltip->last_failure()),
        "Facade did not coalesce callback refresh and option updates");
    const auto committed=take(tooltip->options());auto invalid=committed;invalid.style.content_padding=10;
    require(!tooltip->set_options(invalid)&&take(tooltip->options())==committed&&take(tooltip->root().snapshot()).opened,
        "Mismatched padding changed a visible Tooltip");
    invalid=committed;invalid.configuration.delay=-1ms;
    require(!tooltip->set_options(invalid)&&take(tooltip->options())==committed,"Invalid timing changed Tooltip options");
    invalid=committed;invalid.target.anchor=lvui::TooltipPoint{std::numeric_limits<double>::quiet_NaN(),0};
    require(!tooltip->set_options(invalid)&&take(tooltip->options())==committed,"Invalid target geometry changed Tooltip options");
    good(tooltip->close());require(!take(tooltip->root().snapshot()).opened&&closed==1,"Facade did not close");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=20,.y=20}));
    good(tooltip->open());require(take(tooltip->root().snapshot()).opened&&opened==2,"Facade manual open failed");
    good(tooltip->set_target({}));require(!take(tooltip->root().snapshot()).opened&&closed==2,"Facade target clear did not close");
    good(outer.clear_content());auto retired=tooltip->refresh();
    auto stale_snapshot=tooltip->snapshot();
    require(!retired&&retired.error().code==ii::core::ErrorCode::closed&&!stale_snapshot&&
        stale_snapshot.error().code==ii::core::ErrorCode::closed,"Retired facade accepted access");
    require(!tooltip->set_options(committed)&&!tooltip->options(),"Retired Tooltip accepted an option update");
    tooltip.reset();tree->close();
    auto without_loop=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(without_loop->content_host({},"no-loop",{100,100}));bool rejected=false;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"root",{100,100});if(!root)return std::unexpected(root.error());
        auto result=TooltipTestFacade::create(builder,*root,"tooltip");
        rejected=!result&&result.error().code==ii::core::ErrorCode::unsupported;return {};
    }));
    require(rejected,"Facade accepted a host without an event loop");without_loop->close();
}
void option_rollback(){
    ii::runtime::EventLoop events(1);ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"outer",{640,480}));ii::gui::Widget anchor;
    std::optional<TooltipTestFacade> tooltip;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"layout",{640,480});if(!root)return std::unexpected(root.error());
        auto button=builder.action_surface(*root,"anchor",{.width=100,.height=30});if(!button)return std::unexpected(button.error());
        anchor=*button;if(auto placed=anchor.set_frame({200,200,100,30});!placed)return placed;
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=0ms;config.hide_delay=0ms;config.maximum_height=100;
        auto created=TooltipTestFacade::create(builder,*root,"tooltip",config);
        if(!created)return std::unexpected(created.error());tooltip=*created;
        auto label=builder.label(tooltip->content_root(),"help",{.text=std::string(1000,'x'),.font_size=13,.selectable=false});
        if(!label)return std::unexpected(label.error());return tooltip->set_target(anchor);
    }));
    (void)take(anchor.snapshot());good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=210,.y=210}));
    require(take(tooltip->root().snapshot()).opened,"Tooltip did not open before the rollback test");
    good(anchor.focus());auto nodes=take(tree->snapshots());auto view=std::ranges::find_if(nodes,[](const auto& value){return value.id.ends_with("tooltip/viewport");});
    require(view!=nodes.end(),"Rollback test viewport was missing");auto viewport=take(tree->find(view->id));
    good(viewport.set_scroll_offset(30));require(take(viewport.snapshot()).scroll_offset==30,"Rollback test content was not scrollable");
    const auto before=take(tooltip->options());auto next=before;
    int failures=0;good(viewport.set_scroll_handler([&](const ii::gui::ScrollViewport&){
        if(failures>0){--failures;throw std::runtime_error("Injected viewport failure");}
    }));
    (void)take(viewport.snapshot());
    auto toggle=before;toggle.style.scroll_content=false;failures=2;
    auto interrupted=tooltip->set_options(toggle);
    auto after_interrupted=tooltip->options();
    if(interrupted||failures!=0||after_interrupted)
        throw std::runtime_error("Tooltip rollback injection: result="+std::to_string(bool(interrupted))+
            " remaining="+std::to_string(failures)+" readable="+std::to_string(bool(after_interrupted))+
            (interrupted?"":" error="+interrupted.error().message));
    good(tooltip->reconcile());
    require(take(tooltip->options())==before&&take(viewport.snapshot()).scroll_offset==30&&take(tooltip->root().snapshot()).opened,
        "Tooltip reconciliation did not restore its committed style and scroll position");
    next.configuration.timeout=1000ms;next.style.material.color=lvui::StyleColor{1,0,0,1};next.style.scroll_content=false;
    auto blocker=events.after(std::chrono::hours(1),[]{});require(bool(blocker),"Could not occupy the scheduler");
    auto rejected=tooltip->set_options(next);
    require(!rejected&&rejected.error().code==ii::core::ErrorCode::full&&take(tooltip->options())==before&&
        take(tooltip->root().snapshot()).opened&&take(viewport.snapshot()).scroll_offset==30,
        "Rejected scheduling did not restore Tooltip configuration, style and scroll position");
    (void)blocker.token.cancel();good(tooltip->set_options(next));
    require(take(tooltip->options())==next&&events.stats().pending==1,"Tooltip options did not recover after scheduler capacity returned");
    good(host.clear_content());require(events.stats().pending==0,"Retired Tooltip retained its updated timeout");
    tooltip.reset();tree->close();
}
void geometry_bounds_and_targetless(){
    require(!lvui::native::tooltip_available_rect(std::optional<ii::gui::Rect>{})&&
        lvui::native::tooltip_available_rect(std::optional{ii::gui::Rect{1,2,3,4}})==lvui::TooltipRect{1,2,3,4},
        "Optional native available rectangles were not preserved");
    ii::runtime::EventLoop events;ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"outer",{640,480}));ii::gui::Widget anchor;
    std::optional<TooltipTestFacade> tooltip;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"layout",{640,480});if(!root)return std::unexpected(root.error());
        auto button=builder.action_surface(*root,"anchor",{.width=100,.height=30});if(!button)return std::unexpected(button.error());
        anchor=*button;if(auto placed=anchor.set_frame({200,90,100,30});!placed)return placed;
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=0ms;config.hide_delay=0ms;
        auto created=TooltipTestFacade::create(builder,*root,"tooltip",config);
        if(!created)return std::unexpected(created.error());tooltip=*created;
        auto label=builder.label(tooltip->content_root(),"help",{.text="Tooltip",.font_size=13,.selectable=false});
        if(!label)return std::unexpected(label.error());return tooltip->set_target(anchor);
    }));
    (void)take(anchor.snapshot());good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=210,.y=100}));
    require(take(tooltip->root().snapshot()).opened,"Tooltip did not open before available-area change");
    auto options=take(tooltip->options());options.target.anchor=lvui::TooltipPoint{10,10};
    options.target.available=lvui::TooltipRect{0,150,640,300};good(tooltip->set_options(options));
    auto placed=take(tooltip->snapshot());require(placed.layout&&placed.layout->tip==lvui::TooltipPoint{210,100}&&
        placed.layout->body.y>=158&&placed.layout->body.y+placed.layout->body.height<=442&&take(tooltip->root().snapshot()).opened,
        "An anchor outside the available body area did not stay visible");
    options.target.available=lvui::TooltipRect{-100,-100,1000,1000};good(tooltip->set_options(options));
    placed=take(tooltip->snapshot());require(placed.layout&&placed.layout->body.x>=8&&placed.layout->body.y>=8&&
        placed.layout->body.x+placed.layout->body.width<=632&&placed.layout->body.y+placed.layout->body.height<=472,
        "Available area was not clipped to the native surface");
    good(tooltip->set_target({}));options.target.available=lvui::TooltipRect{0,0,640,480};
    options.target.anchor=lvui::TooltipPoint{200,200};good(tooltip->set_options(options));good(tooltip->open());
    placed=take(tooltip->snapshot());require(placed.layout&&placed.layout->tip==lvui::TooltipPoint{200,200}&&take(tooltip->root().snapshot()).opened,
        "Targetless manual Tooltip could not use surface coordinates");
    options.target.available=lvui::TooltipRect{0,0,100,100};good(tooltip->set_options(options));
    require(!take(tooltip->root().snapshot()).opened,"A targetless Tooltip stayed open outside its new bounds");
    options.target.available=lvui::TooltipRect{0,0,640,480};good(tooltip->set_options(options));good(tooltip->open());good(tooltip->close());
    options.target.anchor=lvui::TooltipPoint{700,200};good(tooltip->set_options(options));
    require(!tooltip->open()&&!take(tooltip->root().snapshot()).opened,"Out-of-surface manual anchor opened a Tooltip");
    good(host.clear_content());tooltip.reset();tree->close();
}
void reentrant_target(){
    ii::runtime::EventLoop events;ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"outer",{640,480}));ii::gui::Widget first,second;
    std::optional<TooltipTestFacade> tooltip;int opened=0,closed=0;bool switched=false;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"layout",{640,480});if(!root)return std::unexpected(root.error());
        auto a=builder.action_surface(*root,"first",{.width=100,.height=30});if(!a)return std::unexpected(a.error());first=*a;
        auto b=builder.action_surface(*root,"second",{.width=100,.height=30});if(!b)return std::unexpected(b.error());second=*b;
        if(auto placed=first.set_frame({200,180,100,30});!placed)return placed;
        if(auto placed=second.set_frame({400,180,100,30});!placed)return placed;
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=0ms;config.hide_delay=0ms;
        auto created=TooltipTestFacade::create(builder,*root,"tooltip",config,{}, {},
            {.opened=[&]{++opened;if(opened==1)switched=bool(tooltip->set_target(second));},.closed=[&]{++closed;}});
        if(!created)return std::unexpected(created.error());tooltip=*created;
        auto label=builder.label(tooltip->content_root(),"help",{.text="Tooltip",.font_size=13,.selectable=false});
        if(!label)return std::unexpected(label.error());return tooltip->set_target(first);
    }));
    (void)take(first.snapshot());good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=210,.y=190}));
    require(switched&&opened==1&&closed==1&&!take(tooltip->root().snapshot()).opened&&!take(tooltip->last_failure()),
        "A Tooltip callback could not switch the observed target");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=410,.y=190}));
    require(take(tooltip->root().snapshot()).opened&&opened==2,"The replacement Tooltip target did not open");
    good(host.clear_content());tooltip.reset();tree->close();
}
void scrolling(){
    ii::runtime::EventLoop events;ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"outer",{640,480}));
    std::optional<TooltipTestFacade> tooltip;std::vector<ii::gui::Widget> rows;ii::gui::Widget sibling;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"layout",{640,480});if(!root)return std::unexpected(root.error());
        auto action=builder.action_surface(*root,"sibling",{.width=60,.height=30});if(!action)return std::unexpected(action.error());sibling=*action;
        if(auto framed=sibling.set_frame({20,20,60,30});!framed)return framed;
        lvui::TooltipConfiguration config;config.automatic=false;config.timeout=0ms;config.maximum_height=100;
        lvui::native::TooltipTargetOptions target;target.anchor=lvui::TooltipPoint{320,300};
        auto created=TooltipTestFacade::create(builder,*root,"scroll-tip",config,{},target);
        if(!created)return std::unexpected(created.error());tooltip=*created;
        for(int n=0;n<12;++n){
            auto row=builder.action_surface(tooltip->content_root(),"row-"+std::to_string(n),{.width=160,.height=28});
            if(!row)return std::unexpected(row.error());rows.push_back(*row);
        }
        return {};
    }));
    good(tooltip->open());(void)take(tree->drawing_snapshot({640,480}));
    const auto nodes=take(tree->snapshots());const auto node=std::ranges::find_if(nodes,[](const auto& value){return value.id.ends_with("scroll-tip/viewport");});
    require(node!=nodes.end(),"Tooltip scroll viewport is missing");auto viewport=take(tree->find(node->id));
    auto before=take(viewport.snapshot());const auto first=take(rows.front().snapshot());
    require(before.kind==ii::gui::WidgetKind::scroll_view&&before.content_height>before.bounds.height&&before.scroll_offset==0,
        "Default Tooltip did not retain scrollable overflow");
    good(tree->input({.kind=ii::gui::WidgetInputKind::scroll,.x=before.bounds.x+10,.y=before.bounds.y+10,.scroll_y=40}));
    auto scrolled=take(viewport.snapshot());const auto moved=take(rows.front().snapshot());
    require(scrolled.scroll_offset==40&&std::abs(moved.bounds.y-(first.bounds.y-40))<.01&&take(tooltip->root().snapshot()).opened,
        "Wheel input did not move retained Tooltip content");
    good(tree->input({.kind=ii::gui::WidgetInputKind::scroll,.x=scrolled.bounds.x+10,.y=scrolled.bounds.y+10,
        .scroll_y=20,.device=ii::gui::PointerDevice::touch}));
    require(take(viewport.snapshot()).scroll_offset==60,"Touch scroll input did not reach Tooltip content");
    auto options=take(tooltip->options());options.style.scroll_content=false;good(tooltip->set_options(options));
    auto clipped=take(viewport.snapshot());
    require(clipped.scroll_offset==0&&clipped.content_height==clipped.bounds.height&&take(rows.front().snapshot()).id==first.id,
        "Disabling Tooltip scrolling did not clip and reset the existing content");
    good(tree->input({.kind=ii::gui::WidgetInputKind::scroll,.x=clipped.bounds.x+10,.y=clipped.bounds.y+10,.scroll_y=40}));
    require(take(viewport.snapshot()).scroll_offset==0,"Disabled Tooltip scrolling consumed wheel input");
    options.style.scroll_content=true;good(tooltip->set_options(options));good(viewport.set_scroll_offset(100000));
    auto end=take(viewport.snapshot());require(end.scroll_offset>0&&std::abs(end.scroll_offset-(end.content_height-end.bounds.height))<.01,
        "Tooltip offset was not clamped to its content");
    good(tooltip->close());good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=30,.y=30}));
    require(take(sibling.interaction()).hovered,"Pointer did not hover the unrelated control");
    options.style.scroll_content=false;good(tooltip->set_options(options));
    require(take(sibling.interaction()).hovered,"Hidden Tooltip scroll reset cleared an unrelated hover");
    options.style.scroll_content=true;good(tooltip->set_options(options));good(tooltip->open());
    require(take(viewport.snapshot()).scroll_offset==0,"Disabling scrolling while hidden left a stale offset on reopening");
    good(viewport.set_scroll_offset(100000));
    options.configuration.maximum_height=200;good(tooltip->set_options(options));
    auto larger=take(viewport.snapshot());require(larger.bounds.height>scrolled.bounds.height&&
        larger.scroll_offset<=std::max(0.,larger.content_height-larger.bounds.height),"Resizing did not clamp Tooltip scrolling");
    for(std::size_t n=1;n<rows.size();++n)good(rows[n].set_visible(false));
    (void)take(tree->drawing_snapshot({640,480}));auto shorter=take(viewport.snapshot());
    require(shorter.scroll_offset==0&&shorter.content_height<=shorter.bounds.height+.01&&take(rows.front().snapshot()).id==first.id,
        "Shrinking Tooltip content left a stale scroll offset or replaced a row");
    good(host.clear_content());require(!viewport.snapshot(),"Retired Tooltip retained its scroll viewport");tooltip.reset();tree->close();
}
void motion(){
    ii::runtime::EventLoop events(1);ii::runtime::FrameLoop frames(events);
    auto preferences=std::make_shared<ii::gui::MotionPreferences>();ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};hooks.preferences=preferences;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"motion-host",{640,480}));ii::gui::Widget anchor,label;
    std::optional<TooltipTestFacade> tip;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{640,480}));
        anchor=take(builder.action_surface(root,"anchor",{.width=100,.height=30}));good(anchor.set_frame({200,200,100,30}));
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=0ms;config.animation_duration=400ms;
        TooltipTestStyle style;style.motion_enabled=true;style.motion=preferences;
        tip=take(TooltipTestFacade::create(builder,root,"motion-tip",config,style));
        label=take(builder.label(tip->content_root(),"motion-label",{.text="Animated tooltip"}));
        return tip->set_target(anchor);
    }));
    (void)take(tree->drawing_snapshot({640,480}));good(tip->open());
    auto time=ii::runtime::Clock::now();(void)frames.step(time+1h);good(tip->close());
    (void)frames.step(time+2h);good(tip->open());time+=2h;
    auto transform=[&]{
        auto scene=take(label.snapshot_scene());
        for(const auto& layer:scene.layers)for(const auto& text:layer.texts)
            if(text.text=="Animated tooltip")return layer.content_transform;
        throw std::runtime_error("Tooltip motion did not preserve its label");
    };
    (void)frames.step(time+80ms);auto enter=transform();
    require(std::abs(enter.opacity-.5)<.04&&enter.scale>.94&&enter.scale<1.02,
        "Tooltip entry did not animate scale and opacity independently");
    auto committed=take(tip->options());auto rejected_options=committed;
    rejected_options.style.motion_enabled=false;rejected_options.configuration.timeout=1000ms;
    auto blocker=events.after(1h,[]{});require(bool(blocker),"Could not occupy the Tooltip scheduler");
    auto rejected=tip->set_options(rejected_options);auto after_rejection=transform();
    require(!rejected&&rejected.error().code==ii::core::ErrorCode::full&&take(tip->options())==committed&&
            std::abs(after_rejection.opacity-enter.opacity)<.005&&std::abs(after_rejection.scale-enter.scale)<.005,
        "A rejected Tooltip configuration changed its in-flight motion");
    (void)blocker.token.cancel();
    (void)frames.step(time+200ms);auto middle=transform();
    require(std::abs(middle.opacity-1)<.01&&std::abs(middle.scale-1)>.001,
        "Tooltip opacity did not finish before its scale");
    auto options=take(tip->options());options.style.motion_enabled=false;good(tip->set_options(options));
    auto settled=transform();require(std::abs(settled.opacity-1)<.001&&std::abs(settled.scale-1)<.001,
        "Disabling Tooltip motion did not settle its active entry");
    options.style.motion_enabled=true;good(tip->set_options(options));good(tip->close());
    (void)frames.step(time+275ms);auto exit=transform();
    require(std::abs(exit.scale-1)<.001&&std::abs(exit.opacity-.5)<.04,
        "Tooltip exit did not hold scale while fading");
    options.configuration.animation_duration=0ms;good(tip->set_options(options));
    require(!take(tip->root().snapshot()).visible,
        "A zero Tooltip duration did not finish an active exit");
    good(tip->open());require(!take(tip->root().snapshot()).transitioning,
        "A zero Tooltip duration started a transition");
    good(tip->close());options.configuration.animation_duration=400ms;good(tip->set_options(options));
    good(preferences->set_reduced_motion(true));good(tip->open());
    require(!take(tip->root().snapshot()).transitioning,"Tooltip ignored shared reduced motion");
    good(host.clear_content());tip.reset();tree->close();
}
void motion_frame_capacity(){
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events,{.capacity=1});
    auto occupied=frames.add([](const ii::runtime::FrameContext&){});require(bool(occupied),"Could not occupy the frame source");
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto host=take(tree->content_host({},"capacity-host",{640,480}));std::optional<TooltipTestFacade> tip;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{640,480}));
        auto anchor=take(builder.action_surface(root,"anchor",{.width=100,.height=30}));good(anchor.set_frame({200,200,100,30}));
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=0ms;
        TooltipTestStyle style;style.motion_enabled=false;
        tip=take(TooltipTestFacade::create(builder,root,"capacity-tip",config,style));
        take(builder.label(tip->content_root(),"label",{.text="Motion capacity"}));return tip->set_target(anchor);
    }));
    (void)take(tree->drawing_snapshot({640,480}));good(tip->open());
    auto before=take(tip->options());auto candidate=before;candidate.style.motion_enabled=true;candidate.configuration.timeout=1000ms;
    auto rejected=tip->set_options(candidate);
    require(!rejected&&rejected.error().code==ii::core::ErrorCode::full&&take(tip->options())==before&&
            events.stats().pending==0&&take(tip->root().snapshot()).opened,
        "Frame capacity failure did not restore Tooltip configuration and deadline");
    good(host.clear_content());tip.reset();tree->close();good(frames.remove(*occupied));
}
}
int main(){observe();facade();option_rollback();geometry_bounds_and_targetless();reentrant_target();scrolling();motion();motion_frame_capacity();}
