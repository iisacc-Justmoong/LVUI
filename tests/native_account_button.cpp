#include "NativeAccountButton.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
int main(){
    using namespace ii::gui;using View=lvui_generated::NativeAccountButton;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task t){t();});
    auto host=take(tree->content_host({},"host",{200,100}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& b){return View::build(b,{}, {},&scope);}));
    (void)take(tree->drawing_snapshot({200,100}));auto nodes=take(tree->snapshots());
    auto found=std::ranges::find_if(nodes,[](const auto& n){return n.id.ends_with("/account");});
    if(found==nodes.end()||!found->custom_content||found->accessibility_name!="Account")
        throw std::runtime_error("Account button did not retain its accessible composed view");
    auto button=take(tree->find(found->id));good(button.activate());
    if(scope->backend_0.submissions!=1)throw std::runtime_error("Composed button did not deliver its action");
    const auto bounds=take(button.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    if(scope->backend_0.submissions!=2)throw std::runtime_error("Composed avatar intercepted the button pointer action");
    good(host.clear_content());scope.reset();if(button.snapshot())throw std::runtime_error("Account button survived retirement");tree->close();
}
