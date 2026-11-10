#include "NativeMenuDynamicBundle.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix,std::string_view key={}){
    const auto nodes=take(tree->snapshots());const auto marker="key:"+std::string(key)+":";
    const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix)&&
        (key.empty()?node.id.find("key:")==std::string::npos:node.id.find(marker)!=std::string::npos);});
    if(found==nodes.end())throw std::runtime_error("Missing dynamic bundle widget");return take(tree->find(found->id));
}
void gap(const ii::gui::Widget& first,const ii::gui::Widget& second,double expected){
    const auto a=take(first.snapshot()).bounds,b=take(second.snapshot()).bounds;
    require(std::abs((b.y-a.y-a.height)-expected)<.01,"Dynamic bundle order or spacing changed");
}
}
int main(){
    using View=lvui_generated::NativeMenuDynamicBundle;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,500}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto menu=find(tree,"/menu"),before=find(tree,"/before"),a=find(tree,"/insideBefore"),b=find(tree,"/insideAfter"),after=find(tree,"/after");
    auto fallback=find(tree,"/fallback"),rows=find(tree,"/insideRows");auto& model=scope->backend_0;
    good(menu.open_at(10,10));gap(before,a,2);gap(a,fallback,2);gap(fallback,b,2);gap(b,after,2);
    const auto a_id=take(a.snapshot()).id,b_id=take(b.snapshot()).id;
    model.title.set("Updated");require(take(a.snapshot()).text=="Updated"&&take(b.snapshot()).text=="Updated"&&
        take(a.snapshot()).id==a_id&&take(b.snapshot()).id==b_id,"A reactive scalar replaced or failed to update the bundle");
    model.show.set(true);auto optional=find(tree,"/optional");require(!fallback.snapshot(),"A replaced nested conditional stayed alive");
    gap(a,optional,2);gap(optional,b,2);
    model.rows.set({model.a,model.b});auto row_a=find(tree,"/insideRow","61"),row_b=find(tree,"/insideRow","62");
    const auto row_a_id=take(row_a.snapshot()).id;gap(optional,row_a,2);gap(row_a,row_b,2);gap(row_b,b,2);
    good(row_a.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(row_b.snapshot()).focused,"Keyboard navigation missed a repeated item inside a bundle");
    good(row_b.activate());require(model.b->clicks==1&&model.triggered==std::vector<int>{4},"Nested repeated action lost its menu index or callback");
    model.rows.set({model.b,model.a});require(take(rows.keyed_content_keys())==std::vector<std::string>({"b","a"})&&
        take(row_a.snapshot()).id==row_a_id,"Reordering nested rows lost stable identity");
    model.spacing.set(5);gap(a,optional,5);gap(optional,row_b,5);gap(row_b,row_a,5);gap(row_a,b,5);
    model.a->visible.set(false);require(!take(row_a.snapshot()).visible&&take(row_a.snapshot()).id==row_a_id,
        "Hiding a nested row replaced it");
    model.a->visible.set(true);gap(row_b,row_a,5);
    model.show.set(false);fallback=find(tree,"/fallback");require(!optional.snapshot()&&take(row_a.snapshot()).id==row_a_id,
        "Changing the nested condition replaced an independent repeated row");
    gap(a,fallback,5);gap(fallback,row_b,5);
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Root binding graph was retired prematurely");
    const auto menu_height=take(menu.snapshot()).bounds.height,row_height=take(row_a.snapshot()).bounds.height;
    model.a->height.set(8192);
    require(take(row_a.snapshot()).id==row_a_id&&take(row_a.snapshot()).bounds.height==row_height&&
            take(menu.snapshot()).bounds.height==menu_height,
        "An invalid nested row changed committed geometry");
    model.a->height.set(24);require(take(row_a.snapshot()).bounds.height==24&&take(row_a.snapshot()).id==row_a_id,
        "The nested row did not recover after invalid input");
    model.a->height.set(18);
    const auto row_b_id=take(row_b.snapshot()).id;model.a->show.set(false);
    require(!row_a.snapshot()&&take(row_b.snapshot()).id==row_b_id&&take(rows.keyed_content_keys())==std::vector<std::string>({"b","a"}),
        "An inner row condition changed another keyed row or discarded its row identity");
    model.a->show.set(true);row_a=find(tree,"/insideRow","61");
    require(take(row_a.snapshot()).id!=row_a_id&&take(row_b.snapshot()).id==row_b_id,
        "An inner row condition did not remount independently");
    model.rows.set({});require(!row_a.snapshot()&&!row_b.snapshot(),"Removed nested rows remained alive");
    require(model.document_revision.get()==0,"Menu view changes modified document state");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!a.snapshot()&&!b.snapshot(),"Retiring a dynamic bundle retained its scope");tree->close();
}
