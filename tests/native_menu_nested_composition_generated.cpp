#include "NativeMenuNestedComposition.generated.h"
#include <iiGUI/WidgetState.h>
#include <Widgets/AccessibilityGrouping.h>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
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
    if(found==nodes.end())throw std::runtime_error("Missing nested composition widget: "+std::string(suffix));
    return take(tree->find(found->id));
}
void gap(const ii::gui::Widget& first,const ii::gui::Widget& second,double expected){
    const auto a=take(first.snapshot()).bounds,b=take(second.snapshot()).bounds;
    require(std::abs((b.y-a.y-a.height)-expected)<.01,"Nested composition lost order or menu spacing");
}
void grouped_menu_item(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,const ii::gui::Widget& item){
    const auto snapshots=take(tree->snapshots());const auto target=take(item.snapshot()).id;
    const auto found=std::ranges::find_if(snapshots,[&](const auto& value){return value.id==target;});
    require(found!=snapshots.end(),"A menu item was absent from accessibility snapshots");
    const auto groups=ii::gui::detail::accessibility_group_parents(snapshots);const auto parent=groups[std::size_t(found-snapshots.begin())];
    require(parent&&snapshots[*parent].accessibility_semantics&&
            snapshots[*parent].accessibility_semantics->role==ii::gui::AccessibilityRole::menu,
            "A nested menu item was not grouped with its semantic menu");
}
}
int main(){
    using View=lvui_generated::NativeMenuNestedComposition;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{600,600}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    bool wrong_owner=false;std::thread worker([&]{auto result=scope->binding_failures();wrong_owner=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(wrong_owner,"Root binding diagnostics did not report a foreign owner thread");
    auto& model=scope->backend_0;model.rows.set({model.c});model.conditional_rows.set({model.a,model.b});model.more.set(true);
    auto menu=find(tree,"/menu"),before=find(tree,"/before"),after=find(tree,"/after");good(menu.open_at(10,10));
    auto conditional=find(tree,"/nestedBefore"),conditional_end=find(tree,"/nestedAfter");
    auto a=find(tree,"/nestedBefore",{"61"}),a_end=find(tree,"/nestedAfter",{"61"});
    auto b=find(tree,"/nestedBefore",{"62"}),b_end=find(tree,"/nestedAfter",{"62"});
    auto a_item=find(tree,"/nestedItem",{"61","63"}),b_item=find(tree,"/nestedItem",{"62","63"});
    auto raw_a=find(tree,"/rawItem",{"61","63"}),raw_b=find(tree,"/rawItem",{"62","63"});
    grouped_menu_item(tree,a_item);grouped_menu_item(tree,raw_b);
    gap(before,conditional,2);gap(conditional_end,a,2);gap(a_end,b,2);gap(b_end,raw_a,2);gap(raw_a,raw_b,2);gap(raw_b,after,2);
    require(take(a.snapshot()).text=="Alpha"&&take(b.snapshot()).text=="Beta","Repeated bundle lost its row-specific binding");
    const auto a_id=take(a.snapshot()).id,b_id=take(b.snapshot()).id,a_item_id=take(a_item.snapshot()).id,raw_a_id=take(raw_a.snapshot()).id;
    model.a->label.set("A changed");require(take(a.snapshot()).text=="A changed"&&take(b.snapshot()).text=="Beta",
        "A nested bundle read a sibling row's reactive value");
    model.show.set(true);auto a_optional=find(tree,"/nestedOptional",{"61"});gap(a,a_optional,2);gap(a_optional,a_item,2);
    model.conditional_rows.set({model.b,model.a});gap(conditional_end,b,2);gap(b_end,a,2);gap(a_end,raw_b,2);gap(raw_b,raw_a,2);gap(raw_a,after,2);
    require(take(a.snapshot()).id==a_id&&take(b.snapshot()).id==b_id&&take(a_item.snapshot()).id==a_item_id&&take(raw_a.snapshot()).id==raw_a_id,
        "Reordering outer rows replaced nested identities");
    grouped_menu_item(tree,a_item);grouped_menu_item(tree,raw_b);
    model.spacing.set(5);gap(b_end,a,5);gap(a,a_optional,5);gap(a_optional,a_item,5);
    model.more.set(false);require(!conditional.snapshot()&&take(a.snapshot()).id==a_id,"Retiring a conditional branch replaced its siblings");
    gap(before,b,5);model.more.set(true);conditional=find(tree,"/nestedBefore");gap(before,conditional,5);
    good(a_item.activate());require(model.c->clicks==1&&!model.triggered.empty(),"Nested item lost its action");
    const auto committed_height=take(menu.snapshot()).bounds.height,item_height=take(a_item.snapshot()).bounds.height;
    require(take(scope->binding_failures()).empty(),"A healthy nested composition reported a binding failure");
    model.c->height.set(8192);require(take(a_item.snapshot()).bounds.height==item_height&&take(menu.snapshot()).bounds.height==committed_height,
        "A rejected nested item height changed the committed menu");
    require(!take(scope->binding_failures()).empty(),"A nested item failure was hidden from its root scope");
    model.c->height.set(18);require(take(scope->binding_failures()).empty(),"A recovered nested item kept a stale diagnostic");
    model.rows.set({model.c,model.c});
    require(take(a_item.snapshot()).id==a_item_id&&take(raw_a.snapshot()).id==raw_a_id,
        "Duplicate inner keys changed committed nested rows");
    model.rows.set({model.c});require(take(a_item.snapshot()).id==a_item_id,"A nested row did not recover after duplicate keys");
    model.rows.set({});require(!a_item.snapshot()&&!b_item.snapshot()&&!raw_a.snapshot()&&!raw_b.snapshot()&&take(a.snapshot()).id==a_id,
        "Removing inner rows damaged their containing bundles");
    model.rows.set({model.c});auto temporary=find(tree,"/nestedItem",{"61","63"});
    model.c->height.set(8192);require(!take(scope->binding_failures()).empty(),"A recreated row did not report its failure");
    model.rows.set({});require(!temporary.snapshot()&&take(scope->binding_failures()).empty(),"A retired failed row left a stale diagnostic");
    model.c->height.set(18);
    model.conditional_rows.set({model.a});require(!b.snapshot()&&take(a.snapshot()).id==a_id,
        "Removing an outer row damaged the retained row");
    require(model.document_revision.get()==0,"Menu composition changed the document revision");
    auto diagnostics=scope->binding_diagnostics;std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();
    require(weak.expired()&&!a.snapshot()&&take(diagnostics->activation_count())==0,"Retiring nested composition retained its owner or binding registrations");tree->close();
}
