#include "NativeOwnedListItemActions.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(node==nodes.end())throw std::runtime_error("Generated item action did not mount");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemActions;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{450,90}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto primary=find(tree,"/item.actions.slot0"),secondary=find(tree,"/item.actions.slot1"),more=find(tree,"/item.actions.slot2");
    good(primary.activate());good(secondary.activate());good(more.activate());
    require(scope->backend_0.actions==std::vector<std::string>({"primary:Open","secondary:Edit","more:More"}),"Generated item lost its action events");
    auto next=scope->backend_0.primary.get();next.text="Launch";scope->backend_0.primary.set(next);
    good(primary.activate());require(scope->backend_0.actions.back()=="primary:Launch","Reactive item action did not update");
    scope->backend_0.show_secondary.set(false);require(!take(secondary.interaction()).visible,"Reactive item action visibility was ignored");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!primary.snapshot(),"Generated item actions retained their owner");tree->close();
}
