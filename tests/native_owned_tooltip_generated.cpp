#include "NativeOwnedTooltip.generated.h"
#include "NativeOwnedTooltipInline.generated.h"
#include <iiGUI/WidgetState.h>
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
}
int main(){generated();inline_content();}
