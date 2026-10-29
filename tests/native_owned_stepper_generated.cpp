#include "NativeOwnedStepper.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    using View=lvui_generated::NativeOwnedStepper;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,160}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[](const auto& value){return value.id.ends_with("/stepper");});
    require(found!=nodes.end(),"Generated Stepper is missing");auto widget=take(tree->find(found->id));auto snapshot=take(widget.snapshot());
    require(snapshot.accessibility_semantics&&snapshot.accessibility_semantics->role==AccessibilityRole::stepper,
            "Generated Stepper is not LVUI-owned");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=snapshot.bounds.x+8,.y=snapshot.bounds.y+3}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=snapshot.bounds.x+8,.y=snapshot.bounds.y+3}));
    require(scope->backend_0.steps==std::vector<int>{1}&&scope->backend_0.clicks==1&&scope->backend_0.presses==1&&scope->backend_0.releases==1,
            "Generated Stepper lost its callbacks");
    scope->backend_0.arrow.set(lvui::StepperArrow::down);snapshot=take(widget.snapshot());
    require(!snapshot.accessibility_semantics->can_increment&&snapshot.accessibility_semantics->can_decrement,
            "Reactive Stepper direction did not reach accessibility");
    good(tree->input({.kind=WidgetInputKind::increment,.target=snapshot.id}));
    good(tree->input({.kind=WidgetInputKind::decrement,.target=snapshot.id}));
    require(scope->backend_0.steps==std::vector<int>({1,-1}),"Generated Stepper did not constrain accessibility actions");
    auto graph=scope->native_lifetime.lock();scope->backend_0.radius.set(-1);
    require(!take(graph->failures()).empty(),"Invalid reactive Stepper options were accepted");
    scope->backend_0.radius.set(5);require(take(graph->failures()).empty(),"Stepper did not recover after an invalid option");
    scope->backend_0.enabled.set(false);good(tree->input({.kind=WidgetInputKind::decrement,.target=snapshot.id}));
    require(scope->backend_0.steps.size()==2,"Disabled generated Stepper accepted input");
    good(host.clear_content());scope.reset();graph.reset();require(!widget.snapshot(),"Retired generated Stepper remained active");tree->close();
}
