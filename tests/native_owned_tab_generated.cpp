#include "NativeOwnedTab.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(node==nodes.end())throw std::runtime_error("Generated Tab did not mount");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedTab;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,180}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto tab=find(tree,"/tab");auto before=take(tab.snapshot());
    require(before.text=="Overview"&&before.accessibility_name=="Overview tab"&&before.action_role==ActionRole::tab&&before.selected,
            "Generated Tab lost initial text or accessibility");
    good(tab.activate());require(scope->backend_0.clicks==1,"Generated Tab action was lost");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Generated Tab binding graph is unavailable");
    scope->backend_0.name.set("missing");scope->backend_0.text.set("Later");
    require(!take(graph->failures()).empty()&&take(tab.snapshot()).text=="Overview","Invalid icon changed the retained Tab");
    scope->backend_0.name.set("nodesfolder");require(take(graph->failures()).empty()&&take(tab.snapshot()).text=="Later",
            "Generated Tab did not recover after icon failure");
    scope->backend_0.selected.set(false);scope->backend_0.style.set(lvui::TabStyle::surface);
    scope->backend_0.width.set(120);require(take(tab.snapshot()).bounds.width>=120&&!take(tab.snapshot()).selected,
            "Generated Tab did not update selection, style or minimum width");
    scope->backend_0.enabled.set(false);good(tab.activate());require(scope->backend_0.clicks==1,"Disabled generated Tab accepted activation");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!tab.snapshot(),"Generated Tab retained a retired activation");tree->close();
}
