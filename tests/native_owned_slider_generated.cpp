#include "NativeOwnedSlider.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void generated(){
    using View=lvui_generated::NativeOwnedSlider;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{640,420}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("/slider");});
    require(found!=nodes.end(),"Generated Slider is missing");auto slider=take(tree->find(found->id));auto initial=take(slider.snapshot());
    require(initial.kind==WidgetKind::action_surface&&initial.value==5&&initial.accessibility_semantics&&
        initial.accessibility_semantics->role==AccessibilityRole::slider,"Generated Slider is not LVUI-owned");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=initial.bounds.x+91,.y=initial.bounds.y+16}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=initial.bounds.x+91,.y=initial.bounds.y+16}));
    require(take(slider.snapshot()).value==10&&scope->backend_0.value.get()==10&&scope->backend_0.values.size()==1,
        "Generated Slider did not deliver its LVUI movement callback");
    scope->backend_0.value.set(4);scope->backend_0.width.set(120);
    auto changed=take(slider.snapshot());require(changed.value==4&&changed.size.width==120,"Generated Slider did not apply reactive values");
    auto graph=scope->native_lifetime.lock();scope->backend_0.maximum.set(-1);
    require(!take(graph->failures()).empty()&&take(slider.snapshot()).value==4,"Invalid reactive range replaced the Slider state");
    scope->backend_0.maximum.set(10);require(take(graph->failures()).empty(),"Generated Slider did not recover after an invalid range");
    scope->backend_0.enabled.set(false);good(tree->input({.kind=WidgetInputKind::increment,.target=initial.id}));
    require(take(slider.snapshot()).value==4,"Disabled generated Slider accepted accessibility input");
    good(host.clear_content());scope.reset();graph.reset();require(!slider.snapshot(),"Retired generated Slider remained active");tree->close();
}
}
int main(){generated();}
