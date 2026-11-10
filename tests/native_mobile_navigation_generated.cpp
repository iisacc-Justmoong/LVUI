#include "NativeMobileNavigation.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[&](const auto& n){return n.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Navigation fixture widget missing");return take(tree->find(found->id));
}
}
int main(){
    using View=lvui_generated::NativeMobileNavigation;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,180}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    (void)take(tree->drawing_snapshot({500,180}));auto first=find(tree,"/navigation.tab0"),second=find(tree,"/navigation.tab1"),search=find(tree,"/navigation.search");
    check(take(first.snapshot()).checked&&!take(second.snapshot()).checked,"Initial navigation selection failed");
    const auto identity=take(second.snapshot()).id;good(second.activate());
    check(scope->backend_0.activations==std::vector<int>{1}&&take(first.snapshot()).checked,
          "Externally owned selection changed on activation");
    scope->backend_0.current.set(1);
    check(!take(first.snapshot()).checked&&take(second.snapshot()).checked&&take(second.snapshot()).id==identity,
          "External selection rebuilt or failed to update the retained tab");
    good(search.activate());check(scope->backend_0.searches==1,"Independent search activation failed");
    const auto previous=scope->backend_0.items.get();auto invalid=previous;invalid[1].key=invalid[0].key;
    auto graph=scope->native_lifetime.lock();scope->backend_0.items.set(invalid);
    check(!take(graph->failures()).empty()&&take(second.snapshot()).checked,"Invalid keys replaced the active navigation");
    scope->backend_0.items.set(previous);check(take(graph->failures()).empty(),"Navigation failed to recover after invalid data");
    auto missing=previous;missing[1].icon_name="nonexistent-mobile-navigation-icon";scope->backend_0.items.set(missing);
    check(!take(graph->failures()).empty()&&take(second.snapshot()).checked,"Failed icon lookup replaced the active navigation");
    scope->backend_0.items.set(previous);check(take(graph->failures()).empty(),"Navigation failed to recover after icon lookup");
    const auto before_focus=take(tree->drawing_snapshot({500,180}));
    good(first.focus());const auto after_focus=take(tree->drawing_snapshot({500,180}));
    check(after_focus!=before_focus,"Focused tab did not expose its accent focus indicator");
    auto reordered=previous;std::swap(reordered[0],reordered[1]);scope->backend_0.items.set(reordered);
    check(take(second.snapshot()).focused,"A keyed tab lost focus when the model was reordered");
    scope->backend_0.automatic.set(true);good(first.activate());
    check(take(first.snapshot()).checked&&scope->backend_0.activations.back()==0,"Automatic selection failed");
    scope->backend_0.current.set(-1);check(take(first.snapshot()).checked,"Automatic selection did not reconcile an empty selection");
    scope->backend_0.searchItem.set(std::nullopt);check(!take(search.snapshot()).visible&&take(first.snapshot()).checked,
        "Removing search disturbed automatic selection or left search visible");
    scope->backend_0.searchItem.set(lvui::MobileNavigationItem{"search","","generalsearch","Search"});
    scope->backend_0.inset.set(20);check(take(find(tree,"/navigation").snapshot()).size.height==108,"Safe inset was not applied");
    scope->backend_0.width.set(150);(void)take(tree->drawing_snapshot({500,180}));
    auto viewport=find(tree,"/navigation.viewport");check(take(viewport.snapshot()).content_width>take(viewport.snapshot()).bounds.width,
        "Narrow navigation did not preserve scrollable touch areas");
    const auto ios=take(tree->drawing_snapshot({500,180}));scope->backend_0.platform.set(2);
    const auto android=take(tree->drawing_snapshot({500,180}));check(ios!=android,"Android navigation did not apply its platform geometry");
    scope->backend_0.width.set(1);(void)take(tree->drawing_snapshot({500,180}));
    good(host.clear_content());scope.reset();graph.reset();check(!first.snapshot(),"Retired navigation remained active");tree->close();
}
