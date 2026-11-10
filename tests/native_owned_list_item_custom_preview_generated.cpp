#include "NativeOwnedListItemCustomPreview.generated.h"
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
    if(found==nodes.end())throw std::runtime_error("Generated custom preview control was not mounted");return take(tree->find(found->id));
}
bool near(double a,double b){return std::abs(a-b)<.01;}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemCustomPreview;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,320}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    (void)take(tree->drawing_snapshot({500,320}));auto item=find(tree,"/item"),preview=find(tree,"/preview"),footer=find(tree,"/footer");
    require(near(take(item.snapshot()).size.height,171)&&near(take(preview.snapshot()).bounds.width,48),
            "Generated preview and footer did not compose");
    auto natural=find(tree,"/naturalPreview");const auto natural_view=take(natural.snapshot());
    require(near(natural_view.bounds.width,75)&&near(natural_view.bounds.height,55),
            "Generated custom preview did not use its rectangular natural dimensions");
    good(preview.activate());good(footer.activate());require(scope->backend_0.previews==1&&scope->backend_0.footers==1,
            "Generated preview and footer did not reach their owner");
    scope->backend_0.size.set(80);scope->backend_0.label.set("Changed");(void)take(tree->drawing_snapshot({500,320}));
    require(near(take(item.snapshot()).size.height,203)&&near(take(preview.snapshot()).bounds.width,80)&&take(preview.snapshot()).text=="Changed",
            "Reactive custom preview size or label was not applied");
    scope->backend_0.shown.set(false);(void)take(tree->drawing_snapshot({500,320}));
    require(!take(preview.interaction()).visible&&take(footer.interaction()).visible,"Preview visibility affected the footer");
    scope->backend_0.shown.set(true);scope->backend_0.kind.set(lvui::ListItemVariant::media);(void)take(tree->drawing_snapshot({500,320}));
    require(take(preview.interaction()).visible&&take(footer.interaction()).visible,"Custom views were lost after a type change");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!preview.snapshot()&&!footer.snapshot(),"Generated custom views retained their owner");tree->close();
}
