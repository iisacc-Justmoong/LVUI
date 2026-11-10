#include "NativeOwnedSubmenu.generated.h"
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
    const auto nodes=take(tree->snapshots());const auto item=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(item==nodes.end())throw std::runtime_error("Generated submenu node did not mount");return take(tree->find(item->id));
}
void pointer_navigation(){
    using View=lvui_generated::NativeOwnedSubmenu;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto parent=find(tree,"/parent"),more=find(tree,"/more"),child=find(tree,"/child"),leaf=find(tree,"/leaf"),
        middle=find(tree,"/middle"),grandchild=find(tree,"/grandchild"),deep=find(tree,"/deep"),
        other=find(tree,"/other"),other_child=find(tree,"/other-child"),plain=find(tree,"/plain"),
        disabled=find(tree,"/disabled"),disabled_child=find(tree,"/disabled-child");
    good(parent.open_at(20,20));(void)take(tree->drawing_snapshot({500,300}));
    const auto move=[&](Widget target,PointerDevice device=PointerDevice::mouse,std::uint32_t buttons=0){
        const auto bounds=take(target.snapshot()).bounds;
        good(tree->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2,
                          .device=device,.buttons=buttons}));
    };
    move(more,PointerDevice::touch);move(more,PointerDevice::mouse,1);
    require(!take(child.snapshot()).opened,"Touch movement or dragging opened a submenu");
    move(more);require(take(child.snapshot()).opened&&scope->backend_0.opens==1&&scope->backend_0.more==0,
        "Pointer hover did not open the submenu without activating its item");
    move(more);require(scope->backend_0.opens==1,"Repeated pointer movement reopened the same submenu");
    move(other,PointerDevice::touch);move(other,PointerDevice::mouse,1);
    require(take(child.snapshot()).opened&&!take(other_child.snapshot()).opened,
        "Touch movement or dragging switched an open submenu");
    move(leaf);require(take(parent.snapshot()).opened&&take(child.snapshot()).opened,"Entering a submenu closed its parent chain");
    move(middle);require(take(grandchild.snapshot()).opened,"Pointer hover did not open a nested submenu");
    move(deep);require(take(child.snapshot()).opened&&take(grandchild.snapshot()).opened,"Entering a deep submenu closed an ancestor");
    move(other);require(take(parent.snapshot()).opened&&!take(child.snapshot()).opened&&!take(grandchild.snapshot()).opened&&
        take(other_child.snapshot()).opened&&scope->backend_0.other==0&&scope->backend_0.otherOpens==1,
        "Moving to a sibling did not switch the submenu without invoking an action");
    move(plain);require(!take(other_child.snapshot()).opened&&take(parent.snapshot()).opened&&scope->backend_0.plain==0,
        "Moving to an ordinary item did not close the prior submenu without activating it");
    require(scope->backend_0.parentEvents.empty()&&scope->backend_0.childEvents.empty(),
        "Pointer navigation emitted an item-triggered callback");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::up}));
    require(take(other.snapshot()).focused,"Keyboard navigation did not continue from the hovered item");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(other_child.snapshot()).opened,"Keyboard could not open the item after pointer navigation");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::left}));
    require(!take(other_child.snapshot()).opened&&take(other.snapshot()).focused,
        "Keyboard could not return from a pointer-selected submenu");
    move(other);require(take(other_child.snapshot()).opened,"Fixture could not reopen the sibling submenu");
    move(disabled);require(!take(other_child.snapshot()).opened&&!take(disabled_child.snapshot()).opened,
        "Hovering a disabled item did not dismiss its sibling or opened the disabled submenu");
    good(parent.close_popup());good(parent.open_at(20,20));move(more);
    scope->backend_0.onChildClosed=[&]{scope->backend_0.onChildClosed={};good(parent.close_popup());good(parent.open_at(25,25));};
    move(other);require(take(parent.snapshot()).opened&&!take(child.snapshot()).opened&&!take(other_child.snapshot()).opened,
        "An old hover transition opened a submenu in a newly reopened parent");
    move(more);
    scope->backend_0.onChildClosed=[&]{scope->backend_0.onChildClosed={};good(more.activate());};
    move(other);require(take(parent.snapshot()).opened&&take(child.snapshot()).opened&&!take(other_child.snapshot()).opened,
        "An old hover transition replaced a submenu reopened by a close callback");
    good(parent.close_popup());good(parent.open_at(20,20));move(plain);
    const auto bounds=take(plain.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    require(scope->backend_0.plain==1&&!take(parent.snapshot()).opened&&!scope->backend_0.parentEvents.empty()&&
            scope->backend_0.parentEvents.back()==2,"A click after pointer navigation did not invoke and close the ordinary item");
    good(host.clear_content());scope.reset();tree->close();
}
}
int main(){
    using View=lvui_generated::NativeOwnedSubmenu;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto parent=find(tree,"/parent"),more=find(tree,"/more"),child=find(tree,"/child"),leaf=find(tree,"/leaf"),
        middle=find(tree,"/middle"),grandchild=find(tree,"/grandchild"),deep=find(tree,"/deep");
    good(parent.open_at(20,20));require(take(more.snapshot()).focused,"Parent menu did not focus its first item");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Generated submenu graph is unavailable");
    scope->backend_0.hasChildren.set(false);require(!take(graph->failures()).empty(),"Owned submenu accepted hasChildItems false");
    scope->backend_0.hasChildren.set(true);require(take(graph->failures()).empty(),"Submenu did not recover its valid state");
    scope->backend_0.showChevron.set(true);scope->backend_0.showChevron.set(false);
    require(take(graph->failures()).empty(),"Reactive chevron visibility invalidated an attached submenu");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(child.snapshot()).opened&&take(leaf.snapshot()).focused&&scope->backend_0.opens==1&&scope->backend_0.more==0,
        "Right arrow did not open and focus the submenu independently of the action callback");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::left}));
    require(!take(child.snapshot()).opened&&take(parent.snapshot()).opened&&take(more.snapshot()).focused,
        "Left arrow did not return focus to the parent item");
    good(more.activate());require(take(child.snapshot()).opened&&scope->backend_0.more==1&&scope->backend_0.parentEvents==std::vector<int>{0},
        "Activating a submenu item did not open its owned popup");
    const auto leaf_bounds=take(leaf.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=leaf_bounds.x+leaf_bounds.width/2,.y=leaf_bounds.y+leaf_bounds.height/2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=leaf_bounds.x+leaf_bounds.width/2,.y=leaf_bounds.y+leaf_bounds.height/2}));
    require(!take(child.snapshot()).opened&&!take(parent.snapshot()).opened&&scope->backend_0.leaf==1&&
        scope->backend_0.childEvents==std::vector<int>{0}&&scope->backend_0.childCloses==2&&scope->backend_0.parentCloses==1,
        "Leaf action did not dispatch and close its menu chain");
    good(parent.open_at(20,20));good(more.activate());
    scope->backend_0.onLeaf=[&]{scope->backend_0.onLeaf={};good(parent.close_popup());good(parent.open_at(25,25));};
    good(leaf.activate());require(take(parent.snapshot()).opened&&!take(child.snapshot()).opened,
        "A stale child activation closed a reopened parent menu");
    good(more.activate());scope->backend_0.onChildClosed=[&]{scope->backend_0.onChildClosed={};good(more.activate());};
    good(leaf.activate());require(take(parent.snapshot()).opened&&take(child.snapshot()).opened,
        "A stale child close closed the reopened submenu or its parent");
    good(parent.close_popup());
    good(parent.open_at(20,20));good(more.activate());good(middle.activate());
    require(take(grandchild.snapshot()).opened,"A third menu level did not open");good(deep.activate());
    require(scope->backend_0.deep==1&&!take(grandchild.snapshot()).opened&&!take(child.snapshot()).opened&&!take(parent.snapshot()).opened,
        "A deep action did not close all ancestor menus");
    scope->backend_0.moreEnabled.set(false);good(parent.open_at(20,20));good(more.activate());
    require(!take(child.snapshot()).opened,"Disabled submenu owner opened its popup");good(parent.close_popup());
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!child.snapshot(),"Submenu retained its activation");tree->close();
    pointer_navigation();
}
