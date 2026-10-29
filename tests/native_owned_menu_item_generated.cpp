#include "NativeOwnedMenuItem.generated.h"
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
    if(item==nodes.end())throw std::runtime_error("Generated MenuItem did not mount");return take(tree->find(item->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedMenuItem;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,180}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto item=find(tree,"/item"),compact=find(tree,"/compact");auto before=take(item.snapshot());
    require(before.text=="Open"&&before.action_role==ActionRole::menu_item&&before.accessibility_description=="⌘O"&&
            take(compact.snapshot()).bounds.height==18,"Generated MenuItem defaults are incorrect");
    good(item.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(scope->backend_0.clicks==1&&scope->backend_0.presses==1&&scope->backend_0.releases==1,"Generated MenuItem callbacks failed");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Generated MenuItem graph is unavailable");
    scope->backend_0.icon.set("missing");scope->backend_0.label.set("Later");
    require(!take(graph->failures()).empty()&&take(item.snapshot()).text=="Open","Invalid icon replaced the live MenuItem");
    scope->backend_0.icon.set("home");require(take(graph->failures()).empty()&&take(item.snapshot()).text=="Later",
            "Generated MenuItem did not recover after icon failure");
    scope->backend_0.state.set(lvui::MenuEntryState::selected);scope->backend_0.direction.set(lvui::MenuDirection::down);
    require(take(item.snapshot()).selected,"Generated MenuItem did not publish selected state");
    scope->backend_0.visible.set(false);require(!take(item.snapshot()).visible,"Generated MenuItem visibility was not updated");
    scope->backend_0.visible.set(true);scope->backend_0.state.set(lvui::MenuEntryState::inactive);good(item.activate());
    require(scope->backend_0.clicks==1&&!take(item.snapshot()).enabled,"Inactive MenuItem accepted activation");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!item.snapshot(),"Generated MenuItem retained its activation");tree->close();
}
