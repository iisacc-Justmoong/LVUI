#include "NativeFocusRing.generated.h"
#include <iiGUI/WidgetState.h>
#include <memory>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    using View=lvui_generated::NativeFocusRing;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto before=take(tree->drawing_snapshot({200,100}));
    require(before.panels.empty()&&before.overlays.empty(),"Inactive generated focus ring emitted ink");
    scope->backend_0.active.set(true);
    auto after=take(tree->drawing_snapshot({200,100}));
    require(!after.panels.empty()||!after.overlays.empty(),"The focus binding did not update the iiGUI ring");
    auto graph=scope->native_lifetime.lock();scope->backend_0.width.set(-1);
    require(!take(graph->failures()).empty(),"Invalid reactive focus-ring extent was accepted");
    scope->backend_0.width.set(100);
    require(take(graph->failures()).empty(),"Focus ring did not recover after invalid reactive input");
    std::weak_ptr<View::Scope> lifetime=scope;good(host.clear_content());scope.reset();graph.reset();
    require(lifetime.expired(),"A retired focus ring retained its LVUI scope");
    tree->close();
}
