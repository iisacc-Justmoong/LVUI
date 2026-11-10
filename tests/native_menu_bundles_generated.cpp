#include "NativeMenuBundles.generated.h"
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
    const auto found=std::ranges::find_if(nodes,[&](const auto& node){
        return node.id.ends_with(suffix)&&(key.empty()?node.id.find("key:")==std::string::npos:node.id.find(marker)!=std::string::npos);
    });
    if(found==nodes.end())throw std::runtime_error("Missing menu bundle widget");return take(tree->find(found->id));
}
void gap(const ii::gui::Widget& first,const ii::gui::Widget& second,double expected){
    const auto a=take(first.snapshot()).bounds,b=take(second.snapshot()).bounds;
    require(std::abs((b.y-a.y-a.height)-expected)<.01,"Menu bundle spacing diverged from the parent menu");
}
}
int main(){
    using View=lvui_generated::NativeMenuBundles;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,500}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto menu=find(tree,"/menu"),before=find(tree,"/before"),after=find(tree,"/after"),rows=find(tree,"/rows");
    good(menu.open_at(10,10));const auto empty_height=take(menu.snapshot()).bounds.height;auto& model=scope->backend_0;
    model.show.set(true);auto first=find(tree,"/bundleFirst"),divider=find(tree,"/bundleDivider"),second=find(tree,"/bundleSecond");
    const auto first_id=take(first.snapshot()).id;
    require(take(menu.snapshot()).bounds.height==empty_height+45,"A conditional bundle did not add its complete geometry");
    gap(before,first,2);gap(first,divider,2);gap(divider,second,2);gap(second,after,2);
    good(before.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(first.snapshot()).focused,"Keyboard navigation missed the first conditional bundle item");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(second.snapshot()).focused,"Keyboard navigation did not skip the conditional bundle divider");
    good(second.activate());require(model.a->clicks==1&&model.triggered==std::vector<int>{3}&&take(menu.snapshot()).opened,
        "A conditional bundle action did not preserve its menu index and callback");
    model.spacing.set(5);gap(before,first,5);gap(first,divider,5);gap(divider,second,5);gap(second,after,5);
    require(take(first.snapshot()).id==first_id,"Changing menu spacing replaced a conditional bundle item");
    model.a->visible.set(false);
    require(take(menu.snapshot()).bounds.height==empty_height+3,"An entirely hidden bundle left a menu gap");
    model.a->visible.set(true);gap(first,divider,5);
    model.rows.set({model.b,model.c});auto b_first=find(tree,"/bundleFirst","62"),b_second=find(tree,"/bundleSecond","62");
    auto b_divider=find(tree,"/bundleDivider","62");const auto b_id=take(b_first.snapshot()).id;
    gap(b_first,b_divider,5);gap(b_divider,b_second,5);
    const auto with_rows=take(menu.snapshot()).bounds.height;model.b->visible.set(false);
    require(take(menu.snapshot()).bounds.height==with_rows-54&&take(rows.keyed_content_keys())==std::vector<std::string>({"b","c"})&&
            take(b_first.snapshot()).id==b_id,"A hidden repeated bundle retained geometry or lost its keyed identity");
    model.b->visible.set(true);require(take(menu.snapshot()).bounds.height==with_rows&&take(b_first.snapshot()).id==b_id,
            "A repeated bundle did not restore its geometry without remounting");
    const auto bounds=take(b_first.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+4,.y=bounds.y+4}));
    require(take(b_first.snapshot()).selected,"A repeated bundle item could not be selected");
    model.rows.set({model.c,model.b});
    require(take(rows.keyed_content_keys())==std::vector<std::string>({"c","b"})&&take(b_first.snapshot()).id==b_id&&take(b_first.snapshot()).selected,
        "Reordering repeated bundles changed item identity or selection");
    good(b_first.activate());require(model.b->clicks==1,"A repeated bundle item lost its callback");
    model.conditional_rows.set({model.a});auto conditional=find(tree,"/bundleFirst","61");
    const auto conditional_id=take(conditional.snapshot()).id;
    model.a->show.set(false);require(!conditional.snapshot(),"An empty conditional row retained its bundle");
    model.a->height.set(8192);model.a->show.set(true);
    const auto all=take(tree->snapshots());
    require(std::ranges::none_of(all,[](const auto& item){return item.id.find("key:61:")!=std::string::npos&&item.id.ends_with("/bundleFirst");}),
        "An invalid conditional bundle replaced the empty branch");
    model.a->height.set(18);model.a->show.set(false);model.a->show.set(true);conditional=find(tree,"/bundleFirst","61");
    require(take(conditional.snapshot()).id!=conditional_id,"A conditional bundle did not recover after rejection");
    model.spacing.set(8192);
    require(take(b_first.snapshot()).id==b_id,"Rejected spacing replaced a repeated bundle");
    gap(b_first,b_divider,5);gap(b_divider,b_second,5);
    model.spacing.set(5);model.rows.set({});require(!b_first.snapshot()&&take(rows.keyed_content_keys()).empty(),"Removed bundles retained their rows");
    model.conditional_rows.set({});model.show.set(false);require(!conditional.snapshot()&&!first.snapshot(),"Removed branches retained their bundles");
    model.a->height.set(8192);model.show.set(true);
    auto graph=scope->native_lifetime.lock();require(bool(graph)&&!take(graph->failures()).empty(),"An oversized direct bundle did not report failure");
    const auto rejected=take(tree->snapshots());
    require(std::ranges::none_of(rejected,[](const auto& item){return item.id.find("key:")==std::string::npos&&item.id.ends_with("/bundleFirst");}),
        "An invalid direct bundle replaced the empty branch");
    model.a->height.set(18);model.show.set(false);model.show.set(true);first=find(tree,"/bundleFirst");
    require(take(graph->failures()).empty(),"A direct bundle did not recover after rejection");
    model.more.set(true);auto parent=find(tree,"/groupParent"),group_divider=find(tree,"/groupDivider"),peer=find(tree,"/groupPeer");
    auto submenu=find(tree,"/groupSubmenu"),leaf=find(tree,"/groupLeaf");
    const auto parent_bounds=take(parent.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=parent_bounds.x+4,.y=parent_bounds.y+4}));
    require(take(submenu.snapshot()).opened&&model.c->clicks==0,"Hover did not open a grouped submenu safely");
    const auto divider_bounds=take(group_divider.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=divider_bounds.x+4,.y=divider_bounds.y+1}));
    require(!take(submenu.snapshot()).opened,"Hovering over a grouped divider retained the sibling submenu");
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=parent_bounds.x+4,.y=parent_bounds.y+4}));
    require(take(submenu.snapshot()).opened,"A grouped submenu did not reopen after hovering over its divider");
    const auto peer_bounds=take(peer.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=peer_bounds.x+4,.y=peer_bounds.y+4}));
    require(!take(submenu.snapshot()).opened&&take(peer.snapshot()).selected,"Hover did not switch away from a grouped submenu");
    good(parent.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(submenu.snapshot()).opened,"Keyboard navigation did not open a grouped submenu");
    good(leaf.activate());require(model.c->clicks==1&&!take(submenu.snapshot()).opened&&!take(menu.snapshot()).opened,
        "A grouped submenu action did not dispatch or close its ancestors");
    good(menu.open_at(10,10));model.more.set(false);require(!parent.snapshot()&&!peer.snapshot(),"A removed bundle retained its submenu items");
    require(model.document_revision.get()==0,"Menu bundle changes modified document state");
    good(host.clear_content());require(!first.snapshot(),"Retired menu bundles remained live");tree->close();
}
