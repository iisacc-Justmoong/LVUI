#include "NativeOwnedColorButton.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto item=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(item==nodes.end())throw std::runtime_error("Generated ColorPickerButton did not mount");return take(tree->find(item->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedColorButton;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,100}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto source=find(tree,"/source"),direct=find(tree,"/direct");
    require(take(source.snapshot()).accessibility_description=="#FF0000FF"&&take(direct.snapshot()).bounds.width==22&&
        take(direct.snapshot()).accessibility_description=="#0000FFFF","Generated color button defaults are incorrect");
    good(source.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(scope->backend_0.clicks==1&&scope->backend_0.presses==1&&scope->backend_0.releases==1,"Generated color button callbacks failed");
    scope->backend_0.owner->set({0,1,0,.5});scope->backend_0.revision.set(2);scope->backend_0.size.set(36);scope->backend_0.color.set({1,1,0,1});
    require(take(source.snapshot()).accessibility_description=="#00FF0080"&&take(source.snapshot()).bounds.width==36&&
        take(direct.snapshot()).accessibility_description=="#FFFF00FF","Generated color button did not update its source and style");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Generated color button graph is unavailable");
    scope->backend_0.source.set(std::shared_ptr<const lvui::ColorSource>{});
    require(!take(graph->failures()).empty()&&take(source.snapshot()).accessibility_description=="#00FF0080","A missing source replaced the button color");
    scope->backend_0.source.set(scope->backend_0.owner);require(take(graph->failures()).empty(),"Color source did not recover after replacement");
    scope->backend_0.owner->set({2,0,0,1});scope->backend_0.revision.set(3);
    require(!take(graph->failures()).empty()&&take(source.snapshot()).accessibility_description=="#00FF0080","Invalid source color replaced the button");
    scope->backend_0.owner->set({1,0,1,1});scope->backend_0.revision.set(4);
    require(take(graph->failures()).empty()&&take(source.snapshot()).accessibility_description=="#FF00FFFF","Valid source did not recover");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!source.snapshot(),"Color button retained its activation");tree->close();
}
