#include "NativeWheelGuard.generated.h"
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
    using View=lvui_generated::NativeWheelGuard;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{240,180}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    (void)take(tree->drawing_snapshot({240,180}));auto nodes=take(tree->snapshots());
    auto node=std::ranges::find_if(nodes,[](const auto& item){return item.id.ends_with("/scroll");});
    require(node!=nodes.end(),"Generated scroll target is missing");auto scroll=take(tree->find(node->id));auto before=take(scroll.snapshot());
    good(tree->input({.kind=WidgetInputKind::scroll,.x=before.bounds.x+5,.y=before.bounds.y+5,.scroll_y=25}));
    require(take(scroll.snapshot()).scroll_offset==25&&scope->backend_0.events==1,"Generated wheel guard did not route the event");
    scope->backend_0.enabled.set(false);
    good(tree->input({.kind=WidgetInputKind::scroll,.x=before.bounds.x+5,.y=before.bounds.y+5,.scroll_y=20}));
    require(take(scroll.snapshot()).scroll_offset==45&&scope->backend_0.events==1,"Disabled generated guard blocked native scrolling");
    std::weak_ptr<View::Scope> lifetime=scope;good(host.clear_content());scope.reset();
    require(lifetime.expired()&&!scroll.snapshot(),"Retired wheel guard retained its scope");tree->close();
}
