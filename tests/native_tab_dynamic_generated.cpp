#include "NativeTabDynamic.generated.h"
#include <iiGUI/WidgetState.h>
#include <Widgets/AccessibilityGrouping.h>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix,
                     std::initializer_list<std::string_view> keys={}){
    const auto nodes=take(tree->snapshots());
    const auto found=std::ranges::find_if(nodes,[&](const auto& node){
        if(!node.id.ends_with(suffix))return false;
        if(keys.size()==0)return node.id.find("key:")==std::string::npos;
        return std::ranges::all_of(keys,[&](const auto key){return node.id.find("key:"+std::string(key)+":")!=std::string::npos;});
    });
    if(found==nodes.end())throw std::runtime_error("Missing dynamic tab: "+std::string(suffix));
    return take(tree->find(found->id));
}
void gap(const ii::gui::Widget& first,const ii::gui::Widget& second,double expected){
    const auto a=take(first.snapshot()).bounds,b=take(second.snapshot()).bounds;
    require(std::abs((b.x-a.x-a.width)-expected)<.01,"Dynamic tab order or spacing changed");
}
void grouped(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,const ii::gui::Widget& item){
    const auto nodes=take(tree->snapshots());const auto id=take(item.snapshot()).id;
    const auto found=std::ranges::find_if(nodes,[&](const auto& value){return value.id==id;});
    require(found!=nodes.end(),"A tab was absent from accessibility snapshots");
    const auto groups=ii::gui::detail::accessibility_group_parents(nodes);const auto parent=groups[std::size_t(found-nodes.begin())];
    require(parent&&nodes[*parent].accessibility_semantics&&nodes[*parent].accessibility_semantics->role==ii::gui::AccessibilityRole::tab_group,
        "A nested tab was not grouped with its semantic TabBar");
}
}

int main(){
    using View=lvui_generated::NativeTabDynamic;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{3000,120}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto& model=scope->backend_0;model.rows.set({model.c});model.groups.set({model.a,model.b});model.more.set(true);
    (void)take(tree->drawing_snapshot({3000,120}));
    auto before=find(tree,"/before"),after=find(tree,"/after"),conditional=find(tree,"/nestedBefore"),conditional_end=find(tree,"/nestedAfter");
    auto a=find(tree,"/nestedBefore",{"61"}),a_end=find(tree,"/nestedAfter",{"61"});
    auto b=find(tree,"/nestedBefore",{"62"}),b_end=find(tree,"/nestedAfter",{"62"});
    auto a_item=find(tree,"/nestedItem",{"61","63"}),b_item=find(tree,"/nestedItem",{"62","63"});
    auto raw_a=find(tree,"/rawItem",{"61","63"}),raw_b=find(tree,"/rawItem",{"62","63"});
    grouped(tree,a_item);grouped(tree,raw_b);
    gap(before,conditional,4);gap(conditional_end,a,4);gap(a_end,b,4);gap(b_end,raw_a,4);gap(raw_a,raw_b,4);gap(raw_b,after,4);
    const auto a_id=take(a.snapshot()).id,b_id=take(b.snapshot()).id,a_item_id=take(a_item.snapshot()).id;
    model.a->label.set("A changed");require(take(a.snapshot()).text=="A changed"&&take(b.snapshot()).text=="Beta",
        "A nested tab bundle read a sibling row's value");
    model.show.set(true);auto optional=find(tree,"/nestedOptional",{"61"});gap(a,optional,4);gap(optional,a_item,4);
    good(a.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(optional.snapshot()).focused&&model.index.get()==0,"Keyboard focus did not enter a nested conditional tab");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(a_item.snapshot()).focused&&model.index.get()==0,"Keyboard focus did not enter a nested repeated tab");
    good(a_item.activate());const auto selected=model.index.get();model.selections.clear();
    model.groups.set({model.b,model.a});gap(conditional_end,b,4);gap(b_end,a,4);gap(a_end,raw_b,4);gap(raw_b,raw_a,4);gap(raw_a,after,4);
    require(take(a.snapshot()).id==a_id&&take(b.snapshot()).id==b_id&&take(a_item.snapshot()).id==a_item_id&&take(a_item.snapshot()).selected&&model.index.get()!=selected,
        "Reordering nested tabs lost identity, selection or its model index");
    grouped(tree,a_item);model.spacing.set(6);gap(b_end,a,6);gap(a,optional,6);gap(optional,a_item,6);
    model.more.set(false);require(!conditional.snapshot()&&take(a.snapshot()).id==a_id,"Retiring a conditional branch replaced sibling tabs");
    gap(before,b,6);model.more.set(true);conditional=find(tree,"/nestedBefore");gap(before,conditional,6);
    good(a_item.activate());require(model.c->clicks==2,"Nested tab lost its action");
    model.rows.set({model.c,model.c});require(take(a_item.snapshot()).id==a_item_id,"Duplicate inner keys changed committed tabs");
    model.rows.set({model.c});model.rows.set({});require(!a_item.snapshot()&&!b_item.snapshot()&&!raw_a.snapshot()&&!raw_b.snapshot()&&take(a.snapshot()).id==a_id,
        "Removing inner rows damaged their containing tab bundles");
    model.groups.set({model.a});require(!b.snapshot()&&take(a.snapshot()).id==a_id,"Removing a row damaged the retained row");
    require(model.document_revision.get()==0,"Tab navigation changed the document revision");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();require(weak.expired()&&!a.snapshot(),"Retiring tabs retained their owner");tree->close();
}
