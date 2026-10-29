#include "NativeOwnedListItemSelect.generated.h"
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
    if(node==nodes.end()){std::string ids;for(const auto& value:nodes)ids+=" "+value.id;throw std::runtime_error("Generated selectable item did not mount:"+ids);}return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemSelect;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{450,90}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto control=find(tree,"/control");good(control.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(scope->backend_0.changes==std::vector<int>{2}&&scope->backend_0.edits==std::vector<std::string>{"selectorIndex"}&&
            scope->backend_0.index.get()==2,"Generated selectable row lost its reactive edit");
    scope->backend_0.index.set(0);scope->backend_0.options.set({{"New",true},{"Other",true}});
    auto updated=find(tree,"/control");require(!control.snapshot()&&take(updated.action_options()).text=="New",
            "Generated selectable row did not replace its reactive options");control=updated;good(control.focus());
    scope->backend_0.enabled.set(false);good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::up}));
    require(scope->backend_0.changes.size()==1,"Disabled generated selector changed");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!control.snapshot(),"Generated selectable item retained its owner");tree->close();
}
