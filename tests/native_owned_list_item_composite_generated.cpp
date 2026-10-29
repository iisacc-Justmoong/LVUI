#include "NativeOwnedListItemComposite.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(node==nodes.end())throw std::runtime_error("Generated compound item did not mount");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemComposite;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{350,90}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto root=find(tree,"/item"),toggle=find(tree,"/item.toggle"),check=find(tree,"/item.checkbox");
    require(take(toggle.interaction()).visible&&!take(check.interaction()).visible,"Generated toggle row omitted its control");
    good(toggle.activate());require(scope->backend_0.changes==std::vector<bool>{false}&&scope->backend_0.edits==std::vector<std::string>{"checked"}&&
                                    !scope->backend_0.checked.get(),"Generated toggle did not report its reactive value");
    scope->backend_0.kind.set(lvui::ListItemVariant::checkable);
    require(!take(toggle.interaction()).visible&&take(check.interaction()).visible,"Reactive type did not replace the toggle with a check box");
    good(check.activate());require(scope->backend_0.changes==std::vector<bool>({false,true})&&scope->backend_0.edits.size()==2,
                                   "Generated checkable item lost its value event");
    scope->backend_0.enabled.set(false);good(check.activate());require(scope->backend_0.changes.size()==2,"Disabled compound item changed");
    scope->backend_0.visible.set(false);require(!take(root.interaction()).visible,"Reactive compound visibility was ignored");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!toggle.snapshot(),"Generated compound item retained its owner");tree->close();
}
