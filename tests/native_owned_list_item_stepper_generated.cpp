#include "NativeOwnedListItemStepper.generated.h"
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
    if(node==nodes.end())throw std::runtime_error("Generated list stepper did not mount");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemStepper;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{450,90}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto stepper=find(tree,"/item.stepper");require(take(stepper.interaction()).visible,"Generated stepper was hidden");good(stepper.focus());
    const auto key=[&](WidgetKey value){good(tree->input({.kind=WidgetInputKind::key_down,.key=value}));};
    key(WidgetKey::up);key(WidgetKey::up);key(WidgetKey::up);
    require(scope->backend_0.changes==std::vector<double>({2.5,3})&&scope->backend_0.edits==std::vector<std::string>({"quantity","quantity"})&&
            scope->backend_0.quantity.get()==3,"Generated stepper lost bounds or reactive echo");
    scope->backend_0.enabled.set(false);key(WidgetKey::down);require(scope->backend_0.changes.size()==2,"Disabled generated stepper changed");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!stepper.snapshot(),"Generated list stepper retained its owner");tree->close();
}
