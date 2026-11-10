#include "NativeConditionalMenu.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto item=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(item==nodes.end())throw std::runtime_error("Missing conditional menu widget");return take(tree->find(item->id));
}
}
int main(){
    using View=lvui_generated::NativeConditionalMenu;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto menu=find(tree,"/menu"),first=find(tree,"/first"),secondary=find(tree,"/secondary"),last=find(tree,"/last");
    good(menu.open_at(20,20));const auto initial=take(menu.snapshot()).bounds.height;
    scope->backend_0.third.set(true);auto alternative_divider=find(tree,"/alternateDivider");
    require(!secondary.snapshot()&&take(menu.snapshot()).bounds.height==initial-15,"Else-if did not replace its branch with a divider");
    scope->backend_0.third.set(false);secondary=find(tree,"/secondary");
    require(!alternative_divider.snapshot()&&take(menu.snapshot()).bounds.height==initial,"Else-if did not restore the alternate item");
    scope->backend_0.show.set(true);auto item=find(tree,"/conditional");const auto identity=take(item.snapshot()).id;
    require(take(menu.snapshot()).bounds.height==initial+20,"Conditional item did not resize the menu");
    const auto bounds=take(item.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+4,.y=bounds.y+4}));
    require(take(item.snapshot()).selected&&scope->backend_0.clicks==0,"Pointer did not select the conditional item");
    good(item.activate());require(scope->backend_0.clicks==1&&scope->backend_0.triggered_indices==std::vector<int>{1}&&take(menu.snapshot()).opened,
        "Conditional item did not dispatch its action in order");
    scope->backend_0.itemVisible.set(false);require(take(menu.snapshot()).bounds.height==initial,"Hidden conditional item left a gap");
    scope->backend_0.itemVisible.set(true);require(take(item.snapshot()).id==identity&&take(menu.snapshot()).bounds.height==initial+20,
        "Showing a conditional item changed its identity or geometry");
    scope->backend_0.showDivider.set(true);auto divider=find(tree,"/divider");
    require(take(menu.snapshot()).bounds.height==initial+25,"Conditional divider did not resize the menu");
    scope->backend_0.enabled.set(false);good(first.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(secondary.snapshot()).focused,"Keyboard navigation did not skip a disabled conditional item");
    scope->backend_0.enabled.set(true);
    good(first.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(item.snapshot()).focused,"Keyboard navigation missed a conditional item");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(secondary.snapshot()).focused,"Keyboard navigation did not skip the conditional divider");
    scope->backend_0.height.set(8192);
    require(take(menu.snapshot()).bounds.height==initial+25,"An oversized conditional item changed menu geometry");
    require(take(item.snapshot()).id==identity&&take(item.snapshot()).bounds.height==18,
        "An oversized conditional item changed its widget identity or height");
    scope->backend_0.height.set(24);require(take(menu.snapshot()).bounds.height==initial+31&&take(item.snapshot()).bounds.height==24,
        "Conditional menu did not recover from an invalid item");
    scope->backend_0.height.set(18);
    scope->backend_0.primary.set(true);auto primary=find(tree,"/primary");
    require(!secondary.snapshot()&&take(menu.snapshot()).bounds.height==initial+25,"Else branch did not replace its predecessor in place");
    const auto primary_id=take(primary.snapshot()).id;scope->backend_0.third.set(true);
    require(take(primary.snapshot()).id==primary_id,"An inactive else-if condition rebuilt the selected branch");
    auto nested=find(tree,"/nested"),leaf=find(tree,"/nestedLeaf");
    good(primary.activate());require(take(nested.snapshot()).opened,"A conditional item did not open its submenu");
    good(leaf.activate());require(!take(nested.snapshot()).opened&&!take(menu.snapshot()).opened,
        "A conditional submenu action did not close its menu ancestry");
    good(menu.open_at(20,20));
    scope->backend_0.third.set(false);scope->backend_0.alternate_height.set(8192);scope->backend_0.primary.set(false);
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Conditional menu binding graph retired");
    require(!take(graph->failures()).empty()&&take(primary.snapshot()).id==primary_id&&take(menu.snapshot()).bounds.height==initial+25,
        "An invalid conditional branch replaced the previous submenu item");
    scope->backend_0.alternate_height.set(18);scope->backend_0.primary.set(true);scope->backend_0.primary.set(false);
    secondary=find(tree,"/secondary");
    require(take(graph->failures()).empty()&&!primary.snapshot()&&take(menu.snapshot()).bounds.height==initial+25,
        "A valid conditional branch did not recover after rejection");
    scope->backend_0.primary.set(true);primary=find(tree,"/primary");
    scope->backend_0.show.set(false);
    require(!item.snapshot()&&take(menu.snapshot()).bounds.height==initial+5,"Removing a conditional item retained its geometry");
    scope->backend_0.show.set(true);item=find(tree,"/conditional");
    require(take(item.snapshot()).id!=identity&&take(primary.snapshot()).visible&&take(last.snapshot()).visible,
        "A remounted conditional item reused the retired generation or lost siblings");
    scope->backend_0.showDivider.set(false);
    require(!divider.snapshot()&&take(menu.snapshot()).bounds.height==initial+20,"Removing a conditional divider retained its geometry");
    auto fallback=find(tree,"/localFallback");
    require(NativeConditionalMenuBranch::constructions==0,"An inactive branch constructed its local backend");
    NativeConditionalMenuBranch::reject=true;
    scope->backend_0.local.set(true);
    require(take(fallback.snapshot()).text=="Local fallback"&&!take(scope->binding_failures()).empty(),
        "A failed branch-local constructor replaced the previous menu content");
    NativeConditionalMenuBranch::reject=false;
    scope->backend_0.local.set(false);scope->backend_0.local.set(true);
    auto local=find(tree,"/localAction");const auto local_id=take(local.snapshot()).id;
    require(!fallback.snapshot()&&NativeConditionalMenuBranch::constructions==1&&NativeConditionalMenuBranch::live&&
            take(local.snapshot()).text=="Local action","A branch did not construct its local backend and View alias");
    NativeConditionalMenuBranch::live->title.set("Renamed local");
    require(take(local.snapshot()).id==local_id&&take(local.snapshot()).text=="Renamed local",
        "A branch-local observable did not update its retained menu item");
    scope->backend_0.group.set(true);
    auto grouped=find(tree,"/localGroupItem"),grouped_fallback=find(tree,"/localGroupFallback");
    require(NativeConditionalMenuGroup::live&&take(grouped.snapshot()).text=="Grouped"&&take(grouped_fallback.snapshot()).visible,
        "A branch-local View sequence did not publish its initial conditional entry");
    NativeConditionalMenuGroup::live->title.set("Group changed");
    require(take(grouped.snapshot()).text=="Group changed","A typed local View did not observe its branch backend");
    scope->backend_0.groupDetail.set(true);auto grouped_divider=find(tree,"/localGroupDivider");
    require(!grouped_fallback.snapshot()&&take(grouped_divider.snapshot()).visible&&take(grouped.snapshot()).text=="Group changed",
        "A nested branch-local View sequence did not replace only its conditional entry");
    scope->backend_0.group.set(false);
    require(!grouped.snapshot()&&!grouped_divider.snapshot()&&!NativeConditionalMenuGroup::live,
        "A retired branch-local View sequence or its backend remained in the menu");
    scope->backend_0.local.set(false);
    require(!local.snapshot()&&NativeConditionalMenuBranch::destructions==1&&!NativeConditionalMenuBranch::live&&
            take(find(tree,"/localFallback").snapshot()).text=="Local fallback",
        "A branch-local backend survived its retired menu item");
    auto plain_fallback=find(tree,"/plainFallback");scope->backend_0.plain.set(true);
    auto plain=find(tree,"/plainLocal");
    require(!plain_fallback.snapshot()&&NativeConditionalMenuBranch::constructions==2&&
            NativeConditionalMenuBranch::live&&take(plain.snapshot()).text=="Local action",
        "A regular conditional did not activate its branch-local backend");
    NativeConditionalMenuBranch::live->title.set("Plain changed");
    require(take(plain.snapshot()).text=="Plain changed","A regular branch did not observe its local value");
    scope->backend_0.plain.set(false);
    require(!plain.snapshot()&&NativeConditionalMenuBranch::destructions==2&&!NativeConditionalMenuBranch::live,
        "A regular branch did not retire its local backend");
    scope->backend_0.tab.set(true);auto tab=find(tree,"/localTab");
    require(NativeConditionalMenuBranch::constructions==3&&NativeConditionalMenuBranch::live&&
            take(tab.snapshot()).text=="Local action","A TabBar branch did not publish its local View");
    NativeConditionalMenuBranch::live->title.set("Tab changed");
    require(take(tab.snapshot()).text=="Tab changed","A TabBar branch did not observe its local backend");
    scope->backend_0.tab.set(false);
    require(!tab.snapshot()&&NativeConditionalMenuBranch::destructions==3&&!NativeConditionalMenuBranch::live,
        "A TabBar branch did not retire its local backend");
    scope->backend_0.shadow.set(true);auto shadow=find(tree,"/shadowLocal");
    require(take(shadow.snapshot()).text=="Inner","A nested View did not shadow its branch-local value");
    scope->backend_0.shadow.set(false);require(!shadow.snapshot(),"A shadowing branch did not retire");
    require(scope->backend_0.file_revision.get()==0,"Conditional menu state changed product data");tree->close();
}
