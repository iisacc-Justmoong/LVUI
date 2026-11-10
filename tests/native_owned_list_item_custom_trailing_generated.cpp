#include "NativeOwnedListItemCustomTrailing.generated.h"
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
    if(found==nodes.end())throw std::runtime_error("Generated custom trailing control was not mounted");return take(tree->find(found->id));
}
bool near(double a,double b){return std::abs(a-b)<.01;}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemCustomTrailing;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    (void)take(tree->drawing_snapshot({500,300}));auto leading=find(tree,"/leading"),trailing=find(tree,"/trailing"),footer=find(tree,"/footer");
    require(near(take(trailing.snapshot()).bounds.width,64)&&take(leading.interaction()).visible&&take(footer.interaction()).visible&&
            take(leading.snapshot()).bounds.x+take(leading.snapshot()).bounds.width<=take(trailing.snapshot()).bounds.x,
            "Generated leading, trailing and footer views did not compose");
    auto automatic=find(tree,"/automatic");auto automatic_view=take(automatic.snapshot());
    require(near(automatic_view.bounds.width,106)&&near(automatic_view.bounds.height,55),
            "Generated trailing view did not use its natural dimensions");
    good(leading.activate());good(trailing.activate());good(footer.activate());
    require(scope->backend_0.leading_clicks==1&&scope->backend_0.trailing_clicks==1&&scope->backend_0.footer_clicks==1,
            "Generated custom views did not reach their owner");
    scope->backend_0.width.set(90);scope->backend_0.height.set(30);scope->backend_0.label.set("Changed");
    (void)take(tree->drawing_snapshot({500,300}));
    require(near(take(trailing.snapshot()).bounds.width,90)&&near(take(trailing.snapshot()).bounds.height,30)&&take(trailing.snapshot()).text=="Changed",
            "Reactive trailing dimensions or label were not applied");
    scope->backend_0.kind.set(lvui::ListItemVariant::detail_actions);(void)take(tree->drawing_snapshot({500,300}));
    require(!take(trailing.interaction()).visible&&take(leading.interaction()).visible&&take(footer.interaction()).visible,
            "Compound type did not restore its own controls and retain the other custom views");
    scope->backend_0.kind.set(lvui::ListItemVariant::toggle);(void)take(tree->drawing_snapshot({500,300}));
    require(take(trailing.interaction()).visible,"Returning to a simple row did not restore its trailing view");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!leading.snapshot()&&!trailing.snapshot()&&!footer.snapshot(),"Generated custom views retained their owner");tree->close();
}
