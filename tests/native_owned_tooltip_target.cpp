#include <LVUI/NativeTooltip.h>
#include <iiGUI/WidgetState.h>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>

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
    ii::runtime::EventLoop events;auto scheduler=take(lvui::native::NativeTooltipScheduler::create(events.sender()));
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));ii::gui::Widget first,second,label_widget;int clicks=0;
    std::shared_ptr<lvui::native::NativeTooltipPresentation> port;std::optional<lvui::TooltipController> controller;
    std::optional<lvui::native::NativeTooltipTarget> target;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"layout",{640,480});if(!root)return std::unexpected(root.error());
        auto a=builder.action_surface(*root,"first",{.width=100,.height=30});if(!a)return std::unexpected(a.error());first=*a;
        if(auto frame=first.set_frame({200,200,100,30});!frame)return frame;
        auto b=builder.action_surface(*root,"second",{.width=100,.height=30},[&]{++clicks;});if(!b)return std::unexpected(b.error());second=*b;
        if(auto frame=second.set_frame({400,200,100,30});!frame)return frame;
        auto created=lvui::native::NativeTooltipPresentation::create(builder,*root,"tooltip",{},scheduler.weak_schedule());
        if(!created)return std::unexpected(created.error());port=*created;
        auto label=builder.label(port->content_root(),"tooltip-label",{.text="Helpful text",.font_size=13,.selectable=false});
        if(!label)return std::unexpected(label.error());label_widget=*label;
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=0ms;config.hide_delay=0ms;
        auto owner=lvui::TooltipController::create(port,config);if(!owner)return std::unexpected(owner.error());controller=*owner;
        auto other_controller=lvui::TooltipController::create(std::make_shared<OtherPort>(),config);
        if(!other_controller)return std::unexpected(other_controller.error());
        auto mismatch=lvui::native::NativeTooltipTarget::create(builder,port,*other_controller,scheduler);
        if(mismatch)return ii::core::fail(ii::core::ErrorCode::failed,"Mismatched tooltip port was accepted");
        auto observing=lvui::native::NativeTooltipTarget::create(builder,port,*controller,scheduler);
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
    ii::runtime::EventLoop events;ii::gui::detail::WidgetFrameHooks hooks;
    hooks.events=[&]()->ii::core::Result<ii::runtime::EventLoop*>{return &events;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    auto outer=take(tree->content_host({},"outer",{640,480}));ii::gui::Widget anchor;
    std::optional<lvui::native::NativeTooltip> tooltip;int opened=0,closed=0;bool callback_refresh=false;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"layout",{640,480});if(!root)return std::unexpected(root.error());
        auto action=builder.action_surface(*root,"anchor",{.width=100,.height=30});if(!action)return std::unexpected(action.error());anchor=*action;
        if(auto framed=anchor.set_frame({200,200,100,30});!framed)return framed;
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=0ms;config.hide_delay=0ms;
        auto created=lvui::native::NativeTooltip::create(builder,*root,"tooltip",config,{}, {},
            {.opened=[&]{++opened;callback_refresh=bool(tooltip->refresh());},.closed=[&]{++closed;}});
        if(!created)return std::unexpected(created.error());tooltip=*created;
        auto label=builder.label(tooltip->content_root(),"help",{.text="A helpful tooltip",.font_size=13,.selectable=false});
        if(!label)return std::unexpected(label.error());if(auto refreshed=tooltip->refresh();!refreshed)return refreshed;
        return tooltip->set_target(anchor);
    }));
    (void)take(anchor.snapshot());good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=210,.y=210}));
    require(take(tooltip->root().snapshot()).opened&&opened==1&&callback_refresh&&!take(tooltip->last_failure()),"Facade did not open on hover or coalesce callback refresh");
    good(tooltip->close());require(!take(tooltip->root().snapshot()).opened&&closed==1,"Facade did not close");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=20,.y=20}));
    good(tooltip->open());require(take(tooltip->root().snapshot()).opened&&opened==2,"Facade manual open failed");
    good(tooltip->set_target({}));require(!take(tooltip->root().snapshot()).opened&&closed==2,"Facade target clear did not close");
    good(outer.clear_content());auto retired=tooltip->refresh();
    auto stale_snapshot=tooltip->snapshot();
    require(!retired&&retired.error().code==ii::core::ErrorCode::closed&&!stale_snapshot&&
        stale_snapshot.error().code==ii::core::ErrorCode::closed,"Retired facade accepted access");
    tooltip.reset();tree->close();
    auto without_loop=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(without_loop->content_host({},"no-loop",{100,100}));bool rejected=false;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"root",{100,100});if(!root)return std::unexpected(root.error());
        auto result=lvui::native::NativeTooltip::create(builder,*root,"tooltip");
        rejected=!result&&result.error().code==ii::core::ErrorCode::unsupported;return {};
    }));
    require(rejected,"Facade accepted a host without an event loop");without_loop->close();
}
}
int main(){observe();facade();}
