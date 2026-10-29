#include <LVUI/NativeTooltipPresentation.h>
#include <LVUI/NativeTooltipScheduler.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {
using namespace std::chrono_literals;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void presentation(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));std::optional<std::int64_t> deadline;
    std::shared_ptr<lvui::native::NativeTooltipPresentation> port;std::optional<lvui::TooltipController> controller;
    ii::gui::Widget content;bool candidate_rejected=false;int clicks=0;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"layout",{640,480});if(!root)return std::unexpected(root.error());
        auto under=builder.action_surface(*root,"under",{.width=20,.height=20},[&]{++clicks;});
        if(!under)return std::unexpected(under.error());if(auto frame=under->set_frame({190,190,20,20});!frame)return frame;
        lvui::native::TooltipPresentationStyle invalid;invalid.material.width=20;
        auto rejected=lvui::native::NativeTooltipPresentation::create(builder,*root,"invalid",invalid,
            [](std::optional<std::int64_t>)->ii::core::Result<void>{return {};});
        if(rejected)return ii::core::fail(ii::core::ErrorCode::failed,"Ignored tooltip material extent was accepted");
        auto created=lvui::native::NativeTooltipPresentation::create(builder,*root,"tooltip",{},
            [&](std::optional<std::int64_t> value)->ii::core::Result<void>{deadline=value;return {};});
        if(!created)return std::unexpected(created.error());port=*created;content=port->content_root();
        auto label=builder.label(content,"tooltip-label",{.text="Helpful text",.font_size=13,.selectable=false});
        if(!label)return std::unexpected(label.error());
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=100ms;
        auto owner=lvui::TooltipController::create(port,config);if(!owner)return std::unexpected(owner.error());controller=*owner;
        if(auto geometry=controller->set_geometry({{0,0,640,480},{200,200},{100,30}});!geometry)return geometry;
        auto early=controller->open(0);candidate_rejected=!early&&early.error().code==ii::core::ErrorCode::busy&&!deadline;
        return {};
    }));
    require(candidate_rejected,"An unpublished tooltip was allowed to open or retain a timer");
    good(controller->advance(0,{.target_available=true,.target_engaged=true}));
    auto snapshot=take(port->root().snapshot());require(snapshot.opened&&deadline==100,"Published tooltip did not open with its deadline");
    const auto first=take(controller->snapshot());require(first.layout&&snapshot.bounds.width==first.layout->frame.width&&
        snapshot.bounds.height==first.layout->frame.height,"Tooltip popup did not use LVUI layout");
    auto malformed=*first.layout;malformed.tip.x=malformed.frame.x-100;
    require(!port->present(malformed)&&!port->needs_reconciliation()&&take(port->root().snapshot()).opened,
        "Invalid tooltip outline replaced a valid popup");
    bool wrong_thread=false;std::thread other([&]{auto result=port->set_deadline(200);wrong_thread=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});other.join();
    require(wrong_thread&&deadline==100,"A foreign thread changed the tooltip scheduler");
    auto frame=take(tree->drawing_snapshot({640,480}));
    require(std::ranges::any_of(frame.overlays,[](const auto& layer){return layer.backdrop&&layer.backdrop->mask;}),
        "Tooltip did not submit its own material outline");
    auto style=take(port->style());style.material.color=lvui::StyleColor{1,0,0,1};style.material.tint_opacity=.5;
    good(port->set_style(style));frame=take(tree->drawing_snapshot({640,480}));
    require(std::ranges::any_of(frame.overlays,[](const auto& layer){return layer.backdrop&&layer.backdrop->mask&&
        layer.backdrop->tint.red>.99f&&layer.backdrop->tint.alpha>.49f;}),"Tooltip style did not update its retained material");
    auto invalid_style=style;invalid_style.material.width=20;require(!port->set_style(invalid_style)&&
        take(port->style()).material.color==style.material.color,"Invalid tooltip style replaced the valid style");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=200,.y=195}));
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=200,.y=195}));
    require(clicks==1,"The tooltip tail intercepted underlying input");
    auto before=take(content.snapshot());good(controller->set_geometry({{0,0,640,480},{300,250},{100,30}}));
    auto after=take(content.snapshot());require(after.id==before.id&&take(port->root().snapshot()).opened,"Tooltip reposition replaced retained content");
    good(controller->close(10));require(!take(port->root().snapshot()).opened&&!deadline,"Tooltip close left its popup or timer active");
    good(controller->open(11));require(take(port->root().snapshot()).opened&&deadline==111,"Tooltip could not reopen");
    auto late=controller->weak_tick();std::weak_ptr<lvui::native::NativeTooltipPresentation> weak=port;
    good(outer.clear_content());require(!deadline&&!port->root().snapshot(),"Retirement left the popup or timer active");
    auto result=late(111);require(!result&&result.error().code==ii::core::ErrorCode::closed,"Retired tooltip accepted a late timer");
    controller.reset();port.reset();require(weak.expired(),"Retired tooltip retained its presentation port");tree->close();
}
void retire_during_open(){
    ii::gui::Widget outer;bool retire=false,saw_retirement=false;std::optional<std::int64_t> deadline;
    ii::gui::detail::WidgetFrameHooks hooks;hooks.accessibility_changed=[&]{
        if(retire){retire=false;saw_retirement=true;good(outer.clear_content());}
    };
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},ii::gui::WidgetAppearance{},hooks);
    outer=take(tree->content_host({},"outer",{640,480}));std::shared_ptr<lvui::native::NativeTooltipPresentation> port;
    std::optional<lvui::TooltipController> controller;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"layout",{640,480});if(!root)return std::unexpected(root.error());
        auto created=lvui::native::NativeTooltipPresentation::create(builder,*root,"tooltip",{},
            [&](std::optional<std::int64_t> value)->ii::core::Result<void>{deadline=value;return {};});
        if(!created)return std::unexpected(created.error());port=*created;
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=100ms;
        auto owner=lvui::TooltipController::create(port,config);if(!owner)return std::unexpected(owner.error());controller=*owner;
        return controller->set_geometry({{0,0,640,480},{200,200},{100,30}});
    }));
    retire=true;auto result=controller->advance(0,{.target_available=true,.target_engaged=true});
    require(saw_retirement&&!result&&!deadline&&!port->root().snapshot()&&take(controller->snapshot()).needs_reconciliation,
        "Retirement during presentation committed or retained a tooltip");
    controller.reset();port.reset();tree->close();
}
void event_loop_composition(){
    ii::runtime::EventLoop events;auto scheduler=take(lvui::native::NativeTooltipScheduler::create(events.sender()));
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));std::shared_ptr<lvui::native::NativeTooltipPresentation> port;
    std::optional<lvui::TooltipController> controller;
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"layout",{640,480});if(!root)return std::unexpected(root.error());
        auto created=lvui::native::NativeTooltipPresentation::create(builder,*root,"tooltip",{},scheduler.weak_schedule());
        if(!created)return std::unexpected(created.error());port=*created;
        lvui::TooltipConfiguration config;config.delay=0ms;config.timeout=5000ms;
        auto owner=lvui::TooltipController::create(port,config);if(!owner)return std::unexpected(owner.error());controller=*owner;
        if(auto wake=scheduler.set_wake(controller->weak_tick());!wake)return wake;
        return controller->set_geometry({{0,0,640,480},{200,200},{100,30}});
    }));
    good(controller->advance(take(scheduler.elapsed()),{.target_available=true,.target_engaged=true}));
    require(take(port->root().snapshot()).opened&&events.stats().pending==1,"Tooltip did not schedule on iiSDKCore's event loop");
    good(outer.clear_content());require(events.stats().pending==0,"Retired tooltip kept an event-loop deadline");
    (void)events.poll();controller.reset();port.reset();good(scheduler.retire());tree->close();
}
}
int main(){presentation();retire_during_open();event_loop_composition();}
