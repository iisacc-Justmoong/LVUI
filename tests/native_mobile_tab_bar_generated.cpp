#include "NativeMobileTabBar.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
template<class T>T take(ii::core::Result<T> r){if(!r)throw std::runtime_error(r.error().message);return std::move(*r);}
void good(ii::core::Result<void> r){if(!r)throw std::runtime_error(r.error().message);}
int main(){
    using namespace ii::gui;using View=lvui_generated::NativeMobileTabBar;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,180}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& b){return View::build(b,{}, {},&scope);}));
    (void)take(tree->drawing_snapshot({500,180}));auto nodes=take(tree->snapshots());
    auto found=std::ranges::find_if(nodes,[](const auto& n){return n.id.ends_with("/tabs.tab1");});
    if(found==nodes.end())throw std::runtime_error("Mobile tab was not mounted");
    auto tab=take(tree->find(found->id));good(tab.activate());
    if(scope->backend_0.activations!=std::vector<int>{1}||take(tab.snapshot()).checked)
        throw std::runtime_error("Society-style externally owned selection failed");
    scope->backend_0.current.set(1);if(!take(tab.snapshot()).checked)throw std::runtime_error("Mobile tab did not follow the model");
    good(host.clear_content());scope.reset();if(tab.snapshot())throw std::runtime_error("Mobile tab survived retirement");tree->close();
}
