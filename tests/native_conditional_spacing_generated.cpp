#include "NativeConditionalSpacing.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string_view>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());
    const auto item=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(item==nodes.end())throw std::runtime_error("Missing conditional flow widget");return take(tree->find(item->id));
}
}
int main(){
    using View=lvui_generated::NativeConditionalSpacing;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto flow=find(tree,"/flow"),before=find(tree,"/before"),after=find(tree,"/after"),conditional=find(tree,"/conditional");
    (void)take(tree->drawing_snapshot({400,300}));
    const auto gap=[&](const ii::gui::Widget& a,const ii::gui::Widget& b){
        const auto first=take(a.snapshot()).bounds,second=take(b.snapshot()).bounds;
        return double(second.y-first.y-first.height);
    };
    require(std::abs(gap(before,conditional)-10)<.01&&std::abs(gap(conditional,after)-10)<.01,
        "An active conditional view changed stack spacing");
    const auto old_id=take(conditional.snapshot()).id;
    good(conditional.focus());scope->backend_0.visible.set(false);
    require(!conditional.snapshot()&&std::abs(gap(before,after)-10)<.01,
        "An absent conditional view retained its own stack spacing");
    require(!take(flow.interaction()).focused,"Removing a conditional view retained focus");
    scope->backend_0.visible.set(true);auto replacement=find(tree,"/conditional");
    require(take(replacement.snapshot()).id!=old_id&&
            std::abs(gap(before,replacement)-10)<.01&&std::abs(gap(replacement,after)-10)<.01,
        "A restored conditional view did not recover its natural spacing");
    auto first=find(tree,"/primary"),else_before=find(tree,"/elseBefore"),else_after=find(tree,"/elseAfter");
    const auto first_id=take(first.snapshot()).id;
    scope->backend_0.alternate_height.set(-1);scope->backend_0.primary.set(false);
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Conditional binding graph retired prematurely");
    require(!take(graph->failures()).empty()&&take(first.snapshot()).id==first_id&&
            std::abs(gap(else_before,first)-10)<.01&&std::abs(gap(first,else_after)-10)<.01,
        "A rejected conditional candidate changed the previous branch or its spacing");
    scope->backend_0.alternate_height.set(20);scope->backend_0.primary.set(true);scope->backend_0.primary.set(false);
    auto alternate=find(tree,"/alternate");
    require(take(graph->failures()).empty()&&!first.snapshot()&&
            std::abs(gap(else_before,alternate)-10)<.01&&std::abs(gap(alternate,else_after)-10)<.01,
        "A valid alternate branch did not recover after candidate rejection");
    require(scope->backend_0.file_revision.get()==0,"Conditional layout changed product data");
    tree->close();
}
