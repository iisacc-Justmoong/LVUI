#include <LVUI/NativeEventListener.h>
#include "NativeOwnedEventListener.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <limits>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void events(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));std::optional<lvui::native::NativeEventListener> listener;
    std::vector<ii::gui::WidgetInput> events;std::vector<ii::gui::Rect> sizes;bool rejected_reentry=false;
    lvui::native::EventListenerCallbacks callbacks;
    callbacks.input=[&](const auto& event)->ii::core::Result<bool>{events.push_back(event);auto nested=listener->dispatch(event);
        rejected_reentry=!nested&&nested.error().code==ii::core::ErrorCode::busy;return true;};
    callbacks.resized=[&](ii::gui::Rect value){sizes.push_back(value);};
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{auto stack=builder.vstack({},"layout",{.spacing=0,.alignment=ii::gui::StackAlignment::leading});
        if(!stack)return std::unexpected(stack.error());auto created=lvui::native::NativeEventListener::create(builder,*stack,"listener",{320,180},callbacks);
        if(!created)return std::unexpected(created.error());listener=*created;return {};}));
    auto bounds=take(listener->root().snapshot()).bounds;require(!sizes.empty(),"Published listener did not report its initial layout");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=bounds.x+5,.y=bounds.y+5}));
    require(events.size()==1&&events.back().kind==ii::gui::WidgetInputKind::pointer_down&&rejected_reentry,
        "Listener lost the input event or reentry protection");
    good(listener->set_options({400,240}));(void)take(listener->root().snapshot());
    require(sizes.size()>=2,"Listener did not report a changed extent");
    auto invalid=listener->set_options({0,240});require(!invalid&&listener->root().snapshot(),"Invalid extent retired the listener");
    int replacement=0;good(listener->set_callbacks({.input=[&](const auto&)->ii::core::Result<bool>{++replacement;return false;}}));
    auto dispatched=listener->dispatch({.kind=ii::gui::WidgetInputKind::pointer_move});require(dispatched&&!*dispatched&&replacement==1,
        "Listener callback replacement failed");
    auto bad=ii::gui::WidgetInput{};bad.x=std::numeric_limits<double>::quiet_NaN();require(!listener->dispatch(bad)&&replacement==1,
        "Invalid input reached the callback");
    good(listener->set_callbacks({.input=[](const auto&)->ii::core::Result<bool>{throw std::runtime_error("Callback failed");}}));
    auto failed=listener->dispatch({});require(!failed&&failed.error().cause,"Listener did not preserve a callback exception");
    good(listener->set_callbacks({.input=[&](const auto&)->ii::core::Result<bool>{good(outer.clear_content());return true;}}));
    require(take(listener->dispatch({})),"In-flight listener did not finish after retirement");
    require(!listener->dispatch({.kind=ii::gui::WidgetInputKind::pointer_down}),"Retired listener accepted input");tree->close();
}
void reentrant_updates(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));std::optional<lvui::native::NativeEventListener> listener;
    bool update_options=false,update_callbacks=false;int selected=0;
    lvui::native::EventListenerCallbacks callbacks;
    callbacks.resized=[&](ii::gui::Rect){
        if(update_options){update_options=false;good(listener->set_options({500,300}));}
        if(update_callbacks){update_callbacks=false;good(listener->set_callbacks({.input=[&](const auto&)->ii::core::Result<bool>{selected=2;return true;}}));}
    };
    good(outer.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"layout",{.spacing=0,.alignment=ii::gui::StackAlignment::leading});
        if(!stack)return std::unexpected(stack.error());
        auto created=lvui::native::NativeEventListener::create(builder,*stack,"listener",{320,180},callbacks);
        if(!created)return std::unexpected(created.error());listener=*created;return {};
    }));
    (void)take(listener->root().snapshot());
    good(listener->root().set_extent({321,181}));update_options=true;
    good(listener->set_options({400,240}));auto size=take(listener->root().snapshot()).size;
    require(!update_options&&size.width==500&&size.height==300,"A stale options update replaced the reentrant update");
    good(listener->root().set_extent({480,280}));update_callbacks=true;
    good(listener->set_callbacks({.input=[&](const auto&)->ii::core::Result<bool>{selected=1;return true;}}));
    require(take(listener->dispatch({}))&&selected==2,"A stale callback update replaced the reentrant update");
    bool rejected_options=false,rejected_callbacks=false,rejected_dispatch=false;
    std::thread other([&]{
        auto options=listener->set_options({400,240});rejected_options=!options&&options.error().code==ii::core::ErrorCode::wrong_executor;
        auto replacement=listener->set_callbacks({});rejected_callbacks=!replacement&&replacement.error().code==ii::core::ErrorCode::wrong_executor;
        auto input=listener->dispatch({});rejected_dispatch=!input&&input.error().code==ii::core::ErrorCode::wrong_executor;
    });other.join();
    require(rejected_options&&rejected_callbacks&&rejected_dispatch,"A foreign thread accessed listener state");tree->close();
}
void generated(){
    using View=lvui_generated::NativeOwnedEventListener;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{640,480}));std::shared_ptr<View::Scope> scope;
    good(outer.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("/listener");});
    require(found!=nodes.end(),"Generated listener missing");auto listener=take(tree->find(found->id));auto bounds=take(listener.snapshot()).bounds;
    require(!scope->backend_0.layouts.empty(),"Generated listener lost its layout callback");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=bounds.x+5,.y=bounds.y+5}));
    require(scope->backend_0.events.size()==1&&scope->backend_0.events.back().kind==ii::gui::WidgetInputKind::pointer_down,
        "Generated listener lost its input callback");
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=bounds.x+5,.y=bounds.y+5}));
    nodes=take(tree->snapshots());auto alternate=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("/listener-bool");});
    require(alternate!=nodes.end(),"Generated Bool listener missing");auto bool_listener=take(tree->find(alternate->id));auto alternate_bounds=take(bool_listener.snapshot()).bounds;
    good(tree->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=alternate_bounds.x+5,.y=alternate_bounds.y+5}));
    require(scope->backend_0.bool_count==1,"Bool input result was not accepted");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Generated listener binding graph missing");
    scope->backend_0.width.set(400);(void)take(listener.snapshot());require(take(graph->failures()).empty(),"Valid listener size failed");
    scope->backend_0.width.set(-1);require(!take(graph->failures()).empty()&&listener.snapshot(),"Invalid size changed the live listener");
    scope->backend_0.width.set(360);require(take(graph->failures()).empty(),"Listener size did not recover");
    std::weak_ptr<View::Scope> weak=scope;good(outer.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!listener.snapshot(),"Retired listener retained its generated scope");tree->close();
}
}
int main(){events();reentrant_updates();generated();}
