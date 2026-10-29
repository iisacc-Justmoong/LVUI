#include "NativeOwnedComboBox.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
bool has_text(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& run:frame.texts)if(run.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return true;
    return false;
}
}
int main(){
    using View=lvui_generated::NativeOwnedComboBox;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,160}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto frame=take(tree->drawing_snapshot({320,160}));auto nodes=take(tree->snapshots());
    auto found=std::ranges::find_if(nodes,[](const auto& value){return value.id.ends_with("/combo");});
    require(found!=nodes.end()&&has_text(frame,"Initial")&&has_text(frame,"↕"),"Generated ComboBox did not render");
    auto widget=take(tree->find(found->id));auto b=take(widget.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=b.x+5,.y=b.y+5}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=b.x+5,.y=b.y+5}));
    require(scope->backend_0.clicks==1&&scope->backend_0.presses==1&&scope->backend_0.releases==1,
        "Generated ComboBox lost its callbacks");
    scope->backend_0.text.set("Updated");scope->backend_0.arrow.set(lvui::StepperArrow::down);
    frame=take(tree->drawing_snapshot({320,160}));require(has_text(frame,"Updated")&&has_text(frame,"⌄"),
        "Reactive ComboBox text or indicator was not applied");
    auto graph=scope->native_lifetime.lock();scope->backend_0.width.set(18);
    require(!take(graph->failures()).empty(),"Invalid reactive ComboBox width was accepted");
    scope->backend_0.width.set(120);require(take(graph->failures()).empty(),"ComboBox did not recover after an invalid option");
    scope->backend_0.enabled.set(false);good(widget.activate());require(scope->backend_0.clicks==1,"Disabled ComboBox accepted activation");
    good(host.clear_content());scope.reset();graph.reset();require(!widget.snapshot(),"Retired generated ComboBox remained active");tree->close();
}
