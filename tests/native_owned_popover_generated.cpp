#include "NativeOwnedPopover.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string_view>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto item=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(item==nodes.end())throw std::runtime_error("Generated Popover node did not mount");return take(tree->find(item->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedPopover;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto popup=find(tree,"/popup"),label=find(tree,"/label");good(popup.open_at(40,40));auto first=take(popup.snapshot());
    require(first.opened&&first.bounds.width==180&&first.bounds.height==80&&!first.modal&&first.dim&&
        first.accessibility_name=="Details"&&scope->backend_0.opens==1,"Independent Popover did not open with its configured surface");
    scope->backend_0.width.set(220);scope->backend_0.modal.set(true);scope->backend_0.text.set("Updated");
    auto changed=take(popup.snapshot());require(changed.opened&&changed.bounds.width==220&&changed.modal&&take(label.snapshot()).text=="Updated",
        "Reactive Popover update replaced or lost its content");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Generated Popover graph is unavailable");
    scope->backend_0.width.set(-1);require(!take(graph->failures()).empty()&&take(popup.snapshot()).bounds.width==220,"Invalid Popover geometry changed its surface");
    scope->backend_0.width.set(180);require(take(graph->failures()).empty()&&take(popup.snapshot()).bounds.width==180,"Popover did not recover after invalid geometry");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    require(!take(popup.snapshot()).opened&&scope->backend_0.closes==1&&scope->backend_0.cancels==1,"Popover cancellation callbacks failed");
    good(popup.open_at(40,40));scope->backend_0.onClose=[&]{scope->backend_0.onClose={};good(popup.open_at(50,50));};
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    require(take(popup.snapshot()).opened&&scope->backend_0.opens==3&&scope->backend_0.closes==2&&scope->backend_0.cancels==1,
        "An old cancellation reached a reopened Popover");good(popup.close_popup());
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!popup.snapshot(),"Popover retained its activation");tree->close();
}
