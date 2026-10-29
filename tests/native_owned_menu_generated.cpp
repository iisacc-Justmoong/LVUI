#include "NativeOwnedMenu.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string_view>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto item=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(item==nodes.end())throw std::runtime_error("Generated Menu node did not mount");return take(tree->find(item->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedMenu;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto anchor=find(tree,"/anchor"),menu=find(tree,"/menu"),first=find(tree,"/first"),divider=find(tree,"/divider"),second=find(tree,"/second"),list=find(tree,"/list");
    auto position=take(anchor.snapshot());good(tree->input({.kind=WidgetInputKind::context_request,.x=position.bounds.x+1,.y=position.bounds.y+1,.target=position.id}));
    require(take(menu.snapshot()).opened&&scope->backend_0.opens==1,"Generated context menu did not open");
    scope->backend_0.opacity.set(.5);const auto frame=take(tree->drawing_snapshot({400,300}));const auto bounds=take(menu.snapshot()).bounds;
    require(std::ranges::any_of(frame.overlays,[&](const auto& layer){return std::ranges::any_of(layer.panels,[&](const auto& panel){
        return std::abs(panel.bounds.x-bounds.x)<.01&&std::abs(panel.bounds.y-bounds.y)<.01&&std::abs(panel.bounds.width-bounds.width)<.01&&
            std::abs(panel.bounds.height-bounds.height)<.01&&panel.stroke_width==0&&std::abs(panel.top.alpha-.5)<.001;});}),
        "Reactive menu coating did not update its drawing");
    good(first.activate());require(take(menu.snapshot()).opened&&scope->backend_0.clicks==1&&scope->backend_0.selections==std::vector<int>{0},"Generated keep-open action failed");
    const auto old_height=take(menu.snapshot()).bounds.height;scope->backend_0.thickness.set(4);
    require(take(divider.snapshot()).bounds.height==6&&take(menu.snapshot()).bounds.height==old_height+3,"Menu did not resize for a reactive divider");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Generated Menu graph is unavailable");
    scope->backend_0.thickness.set(8190);
    require(!take(graph->failures()).empty()&&take(divider.snapshot()).bounds.height==6&&take(menu.snapshot()).bounds.height==old_height+3,
        "An oversized reactive divider changed its menu or reported success");
    scope->backend_0.thickness.set(4);require(take(graph->failures()).empty(),"Valid divider metrics did not recover after rejection");
    scope->backend_0.dividerVisible.set(false);require(take(menu.snapshot()).bounds.height==old_height-5,"Hidden divider occupied menu space");
    scope->backend_0.label.set("Renamed");scope->backend_0.width.set(180);scope->backend_0.secondEnabled.set(true);
    good(first.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(second.snapshot()).focused,"Keyboard navigation did not skip the divider");scope->backend_0.index.set(2);
    require(take(first.snapshot()).text=="Renamed"&&take(second.snapshot()).selected&&take(menu.snapshot()).bounds.width==196,
        "Generated menu did not update reactive item and container inputs");
    scope->backend_0.keepOpen.set(false);good(first.activate());
    require(!take(menu.snapshot()).opened&&scope->backend_0.closes==1,"Generated action did not resume automatic closing");
    good(list.open_at(20,20));const auto nodes=take(tree->snapshots());
    auto copy=std::ranges::find_if(nodes,[](const auto& value){return value.text=="Copy"&&value.action_role==ActionRole::menu_item;});
    require(copy!=nodes.end(),"Static item list did not create independent actions");good(take(tree->find(copy->id)).activate());
    require(!take(list.snapshot()).opened&&scope->backend_0.selections.back()==0,"Static item list did not dispatch and close");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!menu.snapshot(),"Generated menu retained its activation");tree->close();
}
