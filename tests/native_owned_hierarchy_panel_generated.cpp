#include "NativeOwnedHierarchyPanel.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
template<class T>T take(lvui::HierarchyResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view name){
    const auto nodes=take(tree->snapshots());auto item=std::ranges::find_if(nodes,[&](const auto& n){return n.accessibility_name==name;});
    if(item==nodes.end())throw std::runtime_error("Generated panel child did not mount: "+std::string(name));return take(tree->find(item->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedHierarchyPanel;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,180}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    (void)take(tree->drawing_snapshot({300,180}));auto root=find(tree,"Root"), tool=find(tree,"Panel home"), add=find(tree,"Add file");
    good(root.activate());good(tool.activate());good(add.activate());
    require(scope->backend_0.activations==1 && scope->backend_0.tool_actions==1 && scope->backend_0.footer_actions==1 &&
            take(scope->backend_0.source->snapshot())->revision==0,"Generated panel lost child actions or changed its document");
    scope->backend_0.footer.set(false);require(!take(add.interaction()).visible,"Generated panel did not hide its footer");
    scope->backend_0.width.set(210);auto graph=scope->native_lifetime.lock();
    require(graph && take(graph->failures()).empty() && root.snapshot(),"Reactive panel update failed or replaced its retained row");
    scope->backend_0.width.set(0);require(!take(graph->failures()).empty() && root.snapshot(),"Invalid panel width replaced the working view");
    scope->backend_0.width.set(220);require(take(graph->failures()).empty(),"Generated panel did not recover after an invalid update");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired() && !root.snapshot(),"Generated hierarchy panel retained its activation");tree->close();
}
