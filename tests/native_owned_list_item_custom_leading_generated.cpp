#include "NativeOwnedListItemCustomLeading.generated.h"
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
    if(found==nodes.end())throw std::runtime_error("Generated custom leading control was not mounted");return take(tree->find(found->id));
}
bool near(double a,double b){return std::abs(a-b)<.01;}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemCustomLeading;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,260}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    (void)take(tree->drawing_snapshot({500,260}));auto leading=find(tree,"/leading"),footer=find(tree,"/footer");
    require(near(take(leading.snapshot()).bounds.width,14)&&take(footer.interaction()).visible,
            "Generated leading and footer views did not compose");
    auto natural=find(tree,"/naturalLeading");const auto natural_view=take(natural.snapshot());
    require(near(natural_view.bounds.width,65)&&near(natural_view.bounds.height,50),
            "Generated custom leading view did not use its rectangular natural dimensions");
    good(leading.activate());good(footer.activate());require(scope->backend_0.leading_clicks==1&&scope->backend_0.footer_clicks==1,
            "Generated leading and footer views did not reach their owner");
    scope->backend_0.size.set(24);scope->backend_0.label.set("Changed");(void)take(tree->drawing_snapshot({500,260}));
    require(near(take(leading.snapshot()).bounds.width,24)&&take(leading.snapshot()).text=="Changed",
            "Reactive leading size or label was not applied");
    scope->backend_0.shown.set(false);(void)take(tree->drawing_snapshot({500,260}));
    require(!take(leading.interaction()).visible&&take(footer.interaction()).visible,"Leading visibility affected the footer");
    scope->backend_0.shown.set(true);scope->backend_0.kind.set(lvui::ListItemVariant::detail_actions);(void)take(tree->drawing_snapshot({500,260}));
    require(take(leading.interaction()).visible&&take(footer.interaction()).visible,"Leading view was lost after a type change");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!leading.snapshot()&&!footer.snapshot(),"Generated custom views retained their owner");tree->close();
}
