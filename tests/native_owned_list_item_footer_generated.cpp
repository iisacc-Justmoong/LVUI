#include "NativeOwnedListItemCustomFooter.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Generated custom footer control was not mounted");return take(tree->find(found->id));
}
bool near(double a,double b){return std::abs(a-b)<.01;}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemCustomFooter;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    (void)take(tree->drawing_snapshot({500,300}));auto item=find(tree,"/item"),button=find(tree,"/extra");
    require(near(take(item.snapshot()).size.height,162)&&take(button.snapshot()).text=="Custom",
            "Generated footer did not supply its natural height and label");
    good(button.activate());require(scope->backend_0.clicks==1,"Generated footer did not invoke its owner");
    scope->backend_0.label.set("Changed");scope->backend_0.height.set(50);
    (void)take(tree->drawing_snapshot({500,300}));
    require(near(take(item.snapshot()).size.height,182)&&take(button.snapshot()).text=="Changed",
            "Reactive footer did not resize and update in place");
    scope->backend_0.kind.set(lvui::ListItemVariant::mini);(void)take(tree->drawing_snapshot({500,300}));
    require(!take(button.interaction()).visible,"A generated mini row displayed its modern footer");
    scope->backend_0.kind.set(lvui::ListItemVariant::detail_actions);(void)take(tree->drawing_snapshot({500,300}));
    require(take(button.interaction()).visible&&near(take(item.snapshot()).size.height,182),"Generated footer did not return with its retained state");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!button.snapshot(),"Generated footer retained its retired owner");tree->close();
}
