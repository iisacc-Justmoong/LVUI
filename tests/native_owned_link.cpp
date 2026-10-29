#include <LVUI/NativeLink.h>
#include "NativeOwnedLink.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
bool near(double a,double b){return std::abs(a-b)<1e-5;}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
struct Target final:lvui::NavigationTarget {
    std::vector<std::string>* events=nullptr;
    bool fail=false;
    ii::core::Result<bool> navigate(const lvui::NavigationRequest& request)override{
        events->push_back("navigate:"+request.href);if(fail)return ii::core::fail(ii::core::ErrorCode::failed,"target failed");return true;
    }
};
bool has_text(const ii::gui::CanvasFrame& frame,std::string_view text){
    const auto contains=[&](const auto& values){return std::ranges::any_of(values,[&](const auto& value){return value.text==text;});};
    if(contains(frame.texts))return true;for(const auto& layer:frame.overlays)if(contains(layer.texts))return true;return false;
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    const auto snapshots=take(state->snapshots());const auto found=std::ranges::find_if(snapshots,[&](const auto& item){return item.id.ends_with(suffix);});
    if(found==snapshots.end())throw std::runtime_error("Generated Link missing");return take(state->find(found->id));
}
void generated(){
    using View=lvui_generated::NativeOwnedLink;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{400,180}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder){return View::build(builder,{},
        [](View::Scope& owner){owner.navigation_context->set_target(owner.backend_0.navigator);},&scope);}));
    auto report=find(state,"/report"),child=find(state,"/child-link"),nested=find(state,"/nested");auto frame=take(state->drawing_snapshot({400,180}));
    require(has_text(frame,"Reports")&&has_text(frame,"Custom child")&&!has_text(frame,"Fallback")&&
        take(report.snapshot()).foreground_color&&near(take(report.snapshot()).foreground_color->red,.2),"Generated Link child, text or foreground state missing");
    good(report.activate());auto& model=scope->backend_0;
    require(model.navigator->requests.size()==1&&model.navigator->requests.back().href=="/reports"&&
        model.navigator->requests.back().params==model.params.get()&&model.events==std::vector<std::string>({"navigated:true","method:/reports","after","clicked"}),
        "Generated Link lost navigation or callback order");
    auto router=std::make_shared<NativeOwnedLinkTarget>();auto page=std::make_shared<NativeOwnedLinkPage>();
    model.router.set(router);model.page.set(page);model.events.clear();good(report.activate());
    require(router->requests.size()==1&&router->requests.back().target==page&&model.navigator->requests.size()==1,
        "Generated reactive router or typed page priority failed");
    model.router.set({});model.page.set({});
    model.href.set("/updated");model.text.set("Updated");model.replace.set(true);model.underline.set(false);model.color.set({.8,.3,.2,1});
    model.events.clear();good(report.activate());require(model.navigator->requests.back().href=="/updated"&&model.navigator->requests.back().replace&&
        model.events==std::vector<std::string>({"navigated:true","method:/updated","after","clicked"})&&
        has_text(take(state->drawing_snapshot({400,180})),"Updated")&&!take(report.snapshot()).text_underline&&
        take(report.snapshot()).foreground_color&&near(take(report.snapshot()).foreground_color->red,.8),
        "Generated reactive Link inputs did not update together");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Link binding graph missing");
    model.href.set(std::string("bad\0path",8));require(!take(graph->failures()).empty(),"Invalid reactive route was accepted");
    good(report.activate());require(model.navigator->requests.back().href=="/updated","Invalid route changed the live Link");
    model.href.set("/recovered");require(take(graph->failures()).empty(),"Link did not recover after invalid route");
    model.enabled.set(false);const auto count=model.navigator->requests.size();good(report.activate());
    require(model.navigator->requests.size()==count&&take(report.snapshot()).foreground_color&&
        near(take(report.snapshot()).foreground_color->alpha,.6),"Disabled generated Link navigated or exposed the wrong color");
    good(child.activate());require(model.navigator->requests.back().href=="/child","Generated child Link did not navigate");
    good(nested.activate());require(model.navigator->requests.back().href=="/nested","Nested Link lost its parent navigation context");
    model.show_nested.set(false);require(!nested.snapshot(),"Conditional Link did not retire");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!report.snapshot(),"Generated Link retained a retired scope");state->close();
}
void action_and_lifetime(){
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{400,180}));auto context=std::make_shared<lvui::NavigationContext>();
    std::vector<std::string> events;auto target=std::make_shared<Target>();target->events=&events;context->set_target(target);
    lvui::LinkConfiguration config;config.text="Reports";config.href="/reports";config.underline=true;
    std::optional<lvui::native::NativeLink> link;ii::gui::Widget root;
    lvui::native::LinkCallbacks callbacks;
    callbacks.pressed=[&]{events.push_back("pressed");};callbacks.released=[&]{events.push_back("released");};
    callbacks.navigated=[&](bool accepted){events.push_back(accepted?"navigated:true":"navigated:false");};
    callbacks.navigation_failed=[&](const auto&){events.push_back("failed");};
    callbacks.method=[&](const auto& event){events.push_back("method:"+event.request.href);};
    callbacks.methods.push_back([&](const auto&){events.push_back("methods");});callbacks.clicked=[&]{events.push_back("clicked");};
    callbacks.canceled=[&]{events.push_back("canceled");auto revised=config;revised.text="Reentrant";revised.enabled=true;good(link->set_options(revised));};
    good(host.set_content([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto created=lvui::native::NativeLink::create(builder,{},"link",context,config,callbacks);if(!created)return std::unexpected(created.error());
        link=*created;root=created->root();return {};}));
    auto snap=take(root.snapshot());require(snap.action_role==ii::gui::ActionRole::link&&snap.text_underline&&snap.enabled,
        "Link accessibility or initial options changed");
    require(has_text(take(state->drawing_snapshot({400,180})),"Reports"),"LVUI Link did not paint fallback text");
    good(root.focus());good(state->input({.kind=ii::gui::WidgetInputKind::key_down,.key=ii::gui::WidgetKey::enter}));
    good(state->input({.kind=ii::gui::WidgetInputKind::key_up,.key=ii::gui::WidgetKey::enter}));
    require(events==std::vector<std::string>({"pressed","released","navigate:/reports","navigated:true","method:/reports","methods","clicked"}),
        "Link keyboard, navigation or callback order changed");
    events.clear();auto disabled=config;disabled.enabled=false;good(link->set_options(disabled));target->fail=true;
    auto failed=link->trigger_navigation();require(!failed&&failed.error().code==ii::core::ErrorCode::failed&&
        events==std::vector<std::string>({"navigate:/reports","failed"}),"Explicit disabled navigation or failure notification changed");
    target->fail=false;good(link->set_options(config));events.clear();
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=snap.bounds.x+5,.y=snap.bounds.y+5}));
    good(link->set_options(disabled));snap=take(root.snapshot());
    require(snap.enabled&&has_text(take(state->drawing_snapshot({400,180})),"Reentrant")&&
        std::ranges::find(events,"canceled")!=events.end(),"Reentrant cancel update was overwritten");
    bool rejected=false;std::thread worker([&]{auto changed=link->set_options(config);rejected=!changed&&changed.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread changed Link");good(host.clear_content());
    auto closed=link->trigger_navigation();require(!closed&&closed.error().code==ii::core::ErrorCode::closed,"Retired Link remained navigable");
    link.reset();state->close();
}
void child_and_retirement(){
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{400,180}));auto context=std::make_shared<lvui::NavigationContext>();
    std::vector<std::string> events;auto target=std::make_shared<Target>();target->events=&events;context->set_target(target);
    lvui::LinkConfiguration config;config.text="Fallback";std::optional<lvui::native::NativeLink> link;ii::gui::Widget root;
    lvui::native::LinkCallbacks callbacks;callbacks.navigated=[&](bool){events.push_back("retire");good(host.clear_content());};
    callbacks.clicked=[&]{events.push_back("unexpected");};
    good(host.set_content([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto created=lvui::native::NativeLink::create(builder,{},"link",context,config,callbacks);if(!created)return std::unexpected(created.error());
        link=*created;root=created->root();auto child=builder.label(root,"child",{.text="Child"});if(!child)return std::unexpected(child.error());return {};}));
    auto frame=take(state->drawing_snapshot({400,180}));require(has_text(frame,"Child")&&!has_text(frame,"Fallback"),"Child slot did not suppress fallback");
    good(root.activate());require(events==std::vector<std::string>({"navigate:/","retire"})&&!root.snapshot(),"Retirement did not cancel remaining callbacks");
    link.reset();state->close();
}
}
int main(){action_and_lifetime();child_and_retirement();generated();}
