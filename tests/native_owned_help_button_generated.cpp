#include "NativeOwnedHelpButton.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool has_text(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& run:frame.texts)if(run.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return true;
    return false;
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Help button was not found");return take(tree->find(found->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedHelpButton;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,150}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto frame=take(tree->drawing_snapshot({300,150}));auto help=find(tree,"/help"),custom=find(tree,"/custom");
    auto snap=take(help.snapshot());require(snap.bounds.width==21&&snap.bounds.height==21&&snap.accessibility_name=="Help"&&has_text(frame,"?"),
        "Default HelpButton lost its design or accessible name");
    good(help.activate());good(custom.activate());require(scope->backend_0.clicks==2,"HelpButton activation failed");
    scope->backend_0.text.set("More");scope->backend_0.width.set(44);frame=take(tree->drawing_snapshot({300,150}));
    require(take(custom.snapshot()).bounds.width==44&&has_text(frame,"More"),"Reactive HelpButton inputs were not applied");
    auto graph=scope->native_lifetime.lock();scope->backend_0.width.set(0);
    require(!take(graph->failures()).empty()&&take(custom.snapshot()).bounds.width==44,"Invalid HelpButton width replaced the live state");
    scope->backend_0.width.set(32);require(take(graph->failures()).empty(),"HelpButton did not recover from invalid options");
    scope->backend_0.enabled.set(false);good(custom.activate());require(scope->backend_0.clicks==2,"Disabled HelpButton accepted activation");
    good(host.clear_content());scope.reset();graph.reset();require(!help.snapshot(),"Retired HelpButton remained active");tree->close();
}
