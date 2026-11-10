#include "NativeMenuSequence.generated.h"
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
    const auto nodes=take(tree->snapshots());
    const auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(node==nodes.end())throw std::runtime_error("Missing menu sequence widget");
    return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeMenuSequence;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto menu=find(tree,"/menu"),before=find(tree,"/before"),first=find(tree,"/sequenceFirst");
    auto divider=find(tree,"/sequenceDivider"),second=find(tree,"/sequenceSecond"),after=find(tree,"/after");
    good(menu.open_at(10,10));
    const auto initial_nodes=take(tree->snapshots());
    std::vector<std::string> sequence_ids;
    for(const auto& node:initial_nodes)if(node.id.ends_with("/sequenceFirst"))sequence_ids.push_back(node.id);
    require(sequence_ids.size()==2&&sequence_ids[0]!=sequence_ids[1],
            "Repeated sequence invocations did not retain distinct widget identities");
    auto another=take(tree->find(sequence_ids[1]));
    require(take(before.snapshot()).bounds.y<take(first.snapshot()).bounds.y&&
            take(first.snapshot()).bounds.y<take(divider.snapshot()).bounds.y&&
            take(divider.snapshot()).bounds.y<take(second.snapshot()).bounds.y&&
            take(second.snapshot()).bounds.y<take(after.snapshot()).bounds.y,
            "Menu sequence did not preserve the surrounding order");
    good(before.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(first.snapshot()).focused,"Keyboard navigation missed the first sequence item");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(second.snapshot()).focused,"Keyboard navigation did not skip the sequence divider");
    good(first.activate());good(second.activate());
    require(scope->backend_0.first_calls==1&&scope->backend_0.second_calls==1&&
            scope->backend_0.triggered==std::vector<int>({1,3})&&take(menu.snapshot()).opened,
            "Menu sequence actions did not retain their callbacks and open policy");
    const auto first_id=take(first.snapshot()).id;const auto height=take(menu.snapshot()).bounds.height;
    scope->backend_0.first_label.set("Changed action");
    require(take(first.snapshot()).text=="Changed action"&&take(first.snapshot()).id==first_id&&
            take(another.snapshot()).text=="Another action",
            "A sequence input did not react without replacing its widget");
    scope->backend_0.show_second.set(false);
    require(!take(second.snapshot()).visible&&take(menu.snapshot()).bounds.height<height,
            "A hidden sequence item retained menu geometry");
    good(first.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(after.snapshot()).focused,"Keyboard navigation did not skip the hidden sequence item");
    scope->backend_0.show_second.set(true);
    require(take(second.snapshot()).visible&&take(menu.snapshot()).bounds.height==height,
            "A sequence item could not be shown again");
    good(host.clear_content());require(!first.snapshot()&&!second.snapshot(),"A retired menu retained its sequence items");
    tree->close();
}
