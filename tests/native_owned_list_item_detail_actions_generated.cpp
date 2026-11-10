#include "NativeOwnedListItemDetailActions.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& item){return item.id.ends_with(suffix);});
    if(node==nodes.end())throw std::runtime_error("Generated detail item node is missing");return take(tree->find(node->id));
}
bool contains(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& run:frame.texts)if(run.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return true;
    return false;
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemDetailActions;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{450,200}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto frame=take(tree->drawing_snapshot({450,200}));
    require(contains(frame,"Project")&&contains(frame,"Illustration")&&contains(frame,"Supporting text"),"Generated detail item omitted its defaults or metadata");
    auto more=find(tree,"/item.actions.slot2");good(more.activate());auto popup=find(tree,"/item.popup");
    require(take(popup.snapshot()).opened,"Generated more action did not open its menu");
    auto old_item=find(tree,"/option-1");good(old_item.activate());
    require(scope->backend_0.events==std::vector<std::string>({"more","1:Delete"}),"Generated detail item lost its menu events");
    scope->backend_0.metadata.set("Animation");frame=take(tree->drawing_snapshot({450,200}));
    require(contains(frame,"Animation")&&!contains(frame,"Illustration"),"Reactive metadata did not repaint");
    scope->backend_0.show_metadata.set(false);frame=take(tree->drawing_snapshot({450,200}));
    require(!contains(frame,"Animation"),"Reactive metadata visibility did not repaint");
    scope->backend_0.options.set({"Publish"});good(more.activate());good(find(tree,"/option-0").activate());
    require(scope->backend_0.events.back()=="0:Publish"&&!old_item.snapshot(),"Reactive menu replacement did not retire its old generation");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired(),"Generated detail item retained its owner");tree->close();
}
