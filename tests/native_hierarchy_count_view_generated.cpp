#include "NativeHierarchyCountView.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){
    if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}
    return std::move(*value);
}
void good(ii::core::Result<void> value){
    if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}
}
ii::gui::Widget find_suffix(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto snapshots=take(tree->snapshots());
    const auto found=std::ranges::find_if(snapshots,[&](const auto& item){return item.id.ends_with(suffix);});
    if(found==snapshots.end())throw std::runtime_error("Hierarchy count View widget did not mount");
    return take(tree->find(found->id));
}
ii::gui::Widget find_text(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view text){
    const auto snapshots=take(tree->snapshots());
    const auto found=std::ranges::find_if(snapshots,[&](const auto& item){return item.text==text;});
    if(found==snapshots.end())throw std::runtime_error("Hierarchy count View text did not mount");
    return take(tree->find(found->id));
}
bool contains_text(const ii::gui::CanvasFrame& frame,std::string_view text){
    const auto has=[&](const auto& values){return std::ranges::any_of(values,[&](const auto& item){return item.text==text;});};
    if(has(frame.texts))return true;
    return std::ranges::any_of(frame.overlays,[&](const auto& layer){return has(layer.texts);});
}
}

int main(){
    using View=lvui_generated::NativeHierarchyCountView;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,160}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
    auto frame=take(tree->drawing_snapshot({400,160}));
    auto badge=find_text(tree,"Two"),outside=find_text(tree,"Outside");
    auto count=find_suffix(tree,"custom.count"),row=find_suffix(tree,"custom.row");
    const auto badge_id=take(badge.snapshot()).id;
    require(badge_id!=take(outside.snapshot()).id&&contains_text(frame,"Two")&&contains_text(frame,"3")&&
            !contains_text(frame,"2"),"Custom and default hierarchy counts were not isolated");
    require(take(count.snapshot()).visible&&take(count.snapshot()).bounds.width==40,
            "Custom count View did not occupy the declared count area");
    scope->backend_0.badge.set("Three");
    require(take(badge.snapshot()).text=="Three"&&take(badge.snapshot()).id==badge_id,
            "Forwarded count View lost its reactive value or identity");
    scope->backend_0.count.set(-1);
    require(!take(count.snapshot()).visible,"A missing count left its custom View visible");
    scope->backend_0.count.set(3);scope->backend_0.countWidth.set(60);
    require(take(count.snapshot()).visible&&take(count.snapshot()).bounds.width==60&&take(badge.snapshot()).id==badge_id,
            "Custom count View did not recover its retained content or size");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Hierarchy count binding graph is unavailable");
    scope->backend_0.count.set(-2);
    require(!take(graph->failures()).empty()&&take(count.snapshot()).visible,
            "An invalid count replaced the last valid custom View");
    scope->backend_0.count.set(4);
    require(take(graph->failures()).empty(),"Hierarchy count View did not recover after invalid input");
    scope->backend_0.enabled.set(false);
    require(!take(count.interaction()).enabled&&!take(badge.interaction()).enabled,
            "Disabled hierarchy row left its custom count View interactive");
    scope->backend_0.enabled.set(true);
    require(take(count.interaction()).enabled,"Custom count View did not recover after row re-enablement");
    good(row.activate());require(scope->backend_0.clicks==1,"Custom count content disconnected row activation");
    std::weak_ptr<View::Scope> weak=scope;
    good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!badge.snapshot(),"Hierarchy count View retained its retired activation");
    tree->close();
}
