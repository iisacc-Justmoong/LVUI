#include "NativeMobileTabs.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
bool text(const ii::gui::CanvasFrame& frame,std::string_view wanted){
    const auto contains=[&](const auto& items){return std::ranges::any_of(items,[&](const auto& item){return item.text==wanted;});};
    return contains(frame.texts)||std::ranges::any_of(frame.overlays,[&](const auto& layer){return contains(layer.texts);});
}
}
int main(){
    using View=lvui_generated::NativeMobileTabs;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[](const auto& n){return n.id.ends_with("/tab");});
    require(node!=nodes.end(),"Generated mobile tab is missing");auto tab=take(tree->find(node->id));
    auto drawing=take(tree->drawing_snapshot({200,100}));
    require(text(drawing,"Home")&&text(drawing,"3")&&text(drawing,"Explore"),"Mobile tab labels or badge are missing");
    good(tab.activate());require(scope->backend_0.clicks==1,"Mobile-tab callback was lost");
    scope->backend_0.label.set("Library");scope->backend_0.badge.set("5");scope->backend_0.selected.set(false);
    drawing=take(tree->drawing_snapshot({200,100}));
    require(text(drawing,"Library")&&text(drawing,"5")&&!text(drawing,"Home"),"Mobile-tab bindings did not update");
    std::weak_ptr<View::Scope> lifetime=scope;good(host.clear_content());scope.reset();
    require(lifetime.expired()&&!tab.snapshot(),"Retired mobile tab retained its scope");tree->close();
}
