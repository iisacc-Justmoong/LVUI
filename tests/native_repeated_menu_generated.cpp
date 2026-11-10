#include "NativeRepeatedMenu.generated.h"
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
    if(item==nodes.end())throw std::runtime_error("Missing repeated menu widget");return take(tree->find(item->id));
}
ii::gui::Widget find_keyed(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view key_hex,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto key="key:"+std::string(key_hex)+":";
    const auto item=std::ranges::find_if(nodes,[&](const auto& node){return node.id.find(key)!=std::string::npos&&node.id.ends_with(suffix);});
    if(item==nodes.end())throw std::runtime_error("Missing conditional repeated menu widget");return take(tree->find(item->id));
}
}
int main(){
    using View=lvui_generated::NativeRepeatedMenu;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto menu=find(tree,"/menu"),first=find(tree,"/first"),rows=find(tree,"/rows");
    good(menu.open_at(20,20));const auto initial=take(menu.snapshot()).bounds.height;
    good(first.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    const auto snapshots=take(tree->snapshots());
    const auto selected=std::ranges::find_if(snapshots,[](const auto& node){return node.id.ends_with("/row")&&node.focused;});
    require(selected!=snapshots.end(),"Keyboard navigation missed the first repeated item");
    auto a=take(tree->find(selected->id));const auto identity=selected->id;
    good(a.activate());require(scope->backend_0.a->clicks==1&&scope->backend_0.triggered==std::vector<int>{1},"Repeated action did not dispatch");
    auto& model=scope->backend_0;model.items.set({model.c,model.a,model.d});
    require(take(a.snapshot()).id==identity&&take(a.snapshot()).selected&&take(rows.keyed_content_keys())==std::vector<std::string>({"c","a","d"}),
        "Reordering did not preserve the selected item and keyed order");
    model.a->visible.set(false);require(take(menu.snapshot()).bounds.height==initial-20,"A hidden repeated item left a gap");
    model.a->visible.set(true);require(take(a.snapshot()).id==identity&&take(menu.snapshot()).bounds.height==initial,"Showing a repeated item changed its identity");
    model.spacing.set(5);require(take(menu.snapshot()).bounds.height==initial+21,"Reactive menu spacing did not reach repeated rows");
    const auto before=take(menu.snapshot()).bounds.height;
    const auto old_y=take(a.snapshot()).bounds.y;
    model.spacing.set(8192);
    require(take(menu.snapshot()).bounds.height==before&&take(a.snapshot()).bounds.y==old_y,
        "A rejected menu spacing changed repeated row geometry");
    model.spacing.set(5);
    model.a->enabled.set(false);auto c=take(rows.keyed_content_item("c","row")),d=take(rows.keyed_content_item("d","row"));
    good(first.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(c.snapshot()).focused,"Keyboard navigation missed the reordered first row");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(d.snapshot()).focused&&!take(a.snapshot()).focused,"Keyboard navigation did not skip a disabled repeated item");
    model.a->enabled.set(true);
    model.items.set({model.c,model.a,model.a});
    require(take(rows.keyed_content_keys())==std::vector<std::string>({"c","a","d"})&&take(menu.snapshot()).bounds.height==before,
        "Duplicate keys changed the active menu");
    model.b->height.set(8192);model.items.set({model.c,model.a,model.d,model.b});
    require(take(rows.keyed_content_keys())==std::vector<std::string>({"c","a","d"})&&take(menu.snapshot()).bounds.height==before,
        "An invalid repeated candidate replaced the active menu");
    model.b->height.set(18);model.items.set({model.c,model.a,model.b});
    require(take(rows.keyed_content_keys())==std::vector<std::string>({"c","a","b"})&&take(a.snapshot()).id==identity,
        "A repeated menu did not recover after rejection");
    model.first.set(1);require(take(rows.keyed_content_keys())==std::vector<std::string>({"a","b"}),"Repeated range was not applied");
    auto separators=find(tree,"/separators");const auto without=take(menu.snapshot()).bounds.height;
    model.separators.set({model.x,model.y});
    require(take(separators.keyed_content_keys())==std::vector<std::string>({"x","y"})&&take(menu.snapshot()).bounds.height==without+16,
        "Repeated dividers did not join the menu layout");
    auto x=take(separators.keyed_content_item("x","separatorRow"));const auto divider_id=take(x.snapshot()).id;
    model.x->visible.set(false);require(take(menu.snapshot()).bounds.height==without+8,"A hidden repeated divider left a gap");
    model.x->visible.set(true);model.separators.set({model.y,model.x});
    require(take(x.snapshot()).id==divider_id&&take(menu.snapshot()).bounds.height==without+16,
        "Reordering repeated dividers changed their identity or extent");
    model.separators.set({});require(take(menu.snapshot()).bounds.height==without&&!x.snapshot(),"Removed dividers retained geometry or a live widget");
    auto nested=find(tree,"/nestedRows");model.nested.set({model.x,model.y});
    auto nested_row=take(nested.keyed_content_item("x","nestedRow"));
    auto submenu=take(nested.keyed_content_item("x","submenu"));auto leaf=take(nested.keyed_content_item("x","leaf"));
    good(nested_row.activate());require(take(submenu.snapshot()).opened,"Repeated item did not open its submenu");
    good(leaf.activate());require(model.x->clicks==1&&!take(submenu.snapshot()).opened&&!take(menu.snapshot()).opened,
        "Repeated submenu action did not dispatch or close its menu ancestry");
    model.nested.set({});good(menu.open_at(20,20));
    auto conditional=find(tree,"/conditionalRows");model.conditional.set({model.x,model.y});
    require(take(conditional.keyed_content_keys())==std::vector<std::string>({"x","y"}),"Conditional repeated keys were not published");
    auto item_x=find_keyed(tree,"78","/conditionItem");
    auto item_y=find_keyed(tree,"79","/conditionItem");
    const auto original_x=model.x->label.get(),original_y=take(item_y.snapshot()).text;
    model.x->label.set("Branch-local X");
    require(take(item_x.snapshot()).text=="Branch-local X"&&take(item_y.snapshot()).text==original_y,
        "A repeated branch-local binding did not stay with its own row");
    model.x->label.set(original_x);
    const auto y_bounds=take(item_y.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=y_bounds.x+4,.y=y_bounds.y+4}));
    require(take(item_y.snapshot()).selected,"Conditional repeated item could not be selected");
    const auto y_id=take(item_y.snapshot()).id;const auto two_items=take(menu.snapshot()).bounds.height;
    const auto x_id=take(item_x.snapshot()).id;model.x->alternate.set(true);
    require(take(item_x.snapshot()).id==x_id,"An inactive row condition rebuilt the selected branch");
    model.x->show.set(false);auto alternate=find_keyed(tree,"78","/conditionAlternate");
    model.x->alternate.set(false);auto divider=find_keyed(tree,"78","/conditionDivider");
    require(!item_x.snapshot()&&take(item_y.snapshot()).id==y_id&&take(item_y.snapshot()).selected&&take(menu.snapshot()).bounds.height==two_items-15,
        "Conditional repeated row did not replace its item with a divider in place");
    require(!alternate.snapshot(),"Conditional repeated else-if branch did not retire");
    model.conditional.set({model.y,model.x});
    require(take(conditional.keyed_content_keys())==std::vector<std::string>({"y","x"}),"Conditional row keys did not reorder");
    require(take(divider.snapshot()).visible,"Reordering hid the retained divider");
    require(take(item_y.snapshot()).id==y_id,"Reordering replaced the retained item");
    require(take(item_y.snapshot()).selected,"Reordering lost the selected conditional item");
    model.x->height.set(8192);model.x->show.set(true);
    require(take(divider.snapshot()).visible&&take(item_y.snapshot()).id==y_id&&take(menu.snapshot()).bounds.height==two_items-15,
        "Invalid conditional repeated candidate replaced the existing row");
    model.x->height.set(18);model.x->show.set(false);model.x->show.set(true);
    item_x=find_keyed(tree,"78","/conditionItem");
    require(!divider.snapshot()&&take(item_y.snapshot()).id==y_id&&take(menu.snapshot()).bounds.height==two_items,
        "Conditional repeated row did not recover after rejection");
    auto retained_row=take(conditional.keyed_content_item("y"));const auto retained_id=take(retained_row.snapshot()).id;
    auto replacement=std::make_shared<NativeRepeatedMenuEntry>("y","Replacement");replacement->show.set(false);
    model.conditional.set({replacement,model.x});
    auto replacement_divider=find_keyed(tree,"79","/conditionDivider");
    require(!item_y.snapshot()&&take(retained_row.snapshot()).id==retained_id&&take(replacement_divider.snapshot()).visible&&take(item_x.snapshot()).visible,
        "Replacing data for an existing key lost its row or failed to update its branch");
    replacement->show.set(true);require(!replacement_divider.snapshot()&&find_keyed(tree,"79","/conditionItem").snapshot().has_value(),
        "A reused row did not observe its replacement data");
    model.conditional.set({});auto optional=find(tree,"/optionalRows");const auto empty=take(menu.snapshot()).bounds.height;
    model.optional.set({model.x});auto optional_item=find_keyed(tree,"78","/optionalItem");
    require(take(menu.snapshot()).bounds.height==empty+23,"An optional repeated item did not enter the menu");
    model.x->show.set(false);
    require(!optional_item.snapshot()&&take(optional.keyed_content_keys())==std::vector<std::string>{"x"}&&take(menu.snapshot()).bounds.height==empty,
        "An empty conditional row left geometry or lost its key");
    model.x->show.set(true);require(take(menu.snapshot()).bounds.height==empty+23,"An optional repeated item did not remount");
    model.optional.set({});
    const auto created=NativeMenuBranchLocal::constructions,destroyed=NativeMenuBranchLocal::destructions;
    model.custom.set({model.x});auto custom=find_keyed(tree,"78","/customItem");
    require(NativeMenuBranchLocal::constructions==created+1,"A custom conditional row did not create its local backend");
    good(custom.activate());require(model.x->clicks==2,"A custom conditional row did not dispatch to its supplied backend");
    model.x->show.set(false);require(!custom.snapshot()&&NativeMenuBranchLocal::destructions==destroyed+1,"A custom conditional branch did not retire its local backend");
    model.x->show.set(true);custom=find_keyed(tree,"78","/customItem");
    require(NativeMenuBranchLocal::constructions==created+2,"A custom conditional branch did not recreate its local backend");
    model.custom.set({});require(NativeMenuBranchLocal::destructions==destroyed+2,"A removed custom row retained its local backend");
    model.custom_nested.set({model.x});auto custom_parent=find_keyed(tree,"78","/customParent");
    auto custom_submenu=find_keyed(tree,"78","/customSubmenu"),custom_leaf=find_keyed(tree,"78","/customLeaf");
    good(custom_parent.activate());require(take(custom_submenu.snapshot()).opened,"A custom conditional item did not open its submenu");
    good(custom_leaf.activate());require(model.x->clicks==3&&!take(menu.snapshot()).opened,"A custom conditional submenu lost its action or close ancestry");
    model.custom_nested.set({});good(menu.open_at(20,20));
    const auto local_created=NativeMenuBranchLocal::constructions,local_destroyed=NativeMenuBranchLocal::destructions;
    model.direct_show.set(true);auto direct=find(tree,"/directLocal");
    require(NativeMenuBranchLocal::latest&&take(direct.snapshot()).text=="Local action","A local menu binding did not receive its initial value");
    NativeMenuBranchLocal::latest->label.set("Updated action");
    require(take(direct.snapshot()).text=="Updated action","A local menu binding did not react to its backend");
    good(direct.activate());require(model.direct_clicks==1&&NativeMenuBranchLocal::constructions==local_created+1,
        "A direct conditional local backend was not created or activated");
    model.direct_show.set(false);require(!direct.snapshot()&&NativeMenuBranchLocal::destructions==local_destroyed+1,
        "A direct conditional local backend did not retire with its branch");
    const auto before_failed_local=take(menu.snapshot()).bounds.height;NativeMenuBranchLocal::fail_next=true;model.direct_show.set(true);
    auto graph=scope->native_lifetime.lock();require(bool(graph)&&!take(graph->failures()).empty()&&take(menu.snapshot()).bounds.height==before_failed_local,
        "A failed local activation changed the menu or hid its error");
    model.direct_show.set(false);model.direct_show.set(true);direct=find(tree,"/directLocal");
    require(take(graph->failures()).empty()&&take(direct.snapshot()).visible,"A failed local branch did not recover");model.direct_show.set(false);
    model.direct_custom.set(true);auto direct_custom=find(tree,"/customItem");good(direct_custom.activate());
    require(model.x->clicks==4,"A direct custom conditional view lost its backend callback");
    model.direct_custom.set(false);require(!direct_custom.snapshot(),"A direct custom conditional view did not retire");
    const auto repeated_created=NativeMenuBranchLocal::constructions,repeated_destroyed=NativeMenuBranchLocal::destructions;
    auto custom_direct_rows=find(tree,"/customDirectRows");model.custom_direct.set({model.x,model.y});
    auto custom_x=find_keyed(tree,"78","/customItem");const auto custom_id=take(custom_x.snapshot()).id;
    good(custom_x.activate());require(model.x->clicks==5&&NativeMenuBranchLocal::constructions==repeated_created+2,
        "A repeated custom view lost its action or local backend");
    model.custom_direct.set({model.y,model.x});
    require(take(custom_direct_rows.keyed_content_keys())==std::vector<std::string>({"y","x"})&&take(custom_x.snapshot()).id==custom_id,
        "A repeated custom view did not retain its identity when reordered");
    model.custom_direct.set({});require(!custom_x.snapshot()&&NativeMenuBranchLocal::destructions==repeated_destroyed+2,
        "Removed repeated custom views retained their local backends");
    model.custom_direct_nested.set({model.x});custom_parent=find_keyed(tree,"78","/customParent");
    custom_submenu=find_keyed(tree,"78","/customSubmenu");custom_leaf=find_keyed(tree,"78","/customLeaf");
    good(custom_parent.activate());require(take(custom_submenu.snapshot()).opened,"A repeated custom view did not open its submenu");
    good(custom_leaf.activate());require(model.x->clicks==6&&!take(menu.snapshot()).opened,"A repeated custom submenu lost its action or close ancestry");
    model.custom_direct_nested.set({});
    good(menu.open_at(20,20));auto static_custom=find(tree,"/staticCustomItem");good(static_custom.activate());
    require(model.x->clicks==7&&take(menu.snapshot()).opened,"A direct reusable menu item lost its callback or policy");
    auto static_divider=find(tree,"/staticCustomDivider"),last=find(tree,"/last");
    require(take(static_divider.snapshot()).visible,"A direct reusable divider did not enter the menu");
    good(static_custom.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(last.snapshot()).focused,"Keyboard navigation did not skip reusable dividers");
    require(model.file_revision.get()==0,"Menu changes modified document data");tree->close();
}
