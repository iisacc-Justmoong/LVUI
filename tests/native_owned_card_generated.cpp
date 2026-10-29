#include "NativeOwnedCard.generated.h"
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
    if(node==nodes.end())throw std::runtime_error("Generated card did not mount");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedCard;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{350,340}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto surface=find(tree,"/card.surface"),menu=find(tree,"/card.menu"),action=find(tree,"/card.action"),root=find(tree,"/card");
    good(surface.activate());require(scope->backend_0.clicks==1 && scope->backend_0.changes==std::vector<bool>{true} && scope->backend_0.selected.get(),
                                    "Generated card did not report selection and activation");
    good(menu.activate());good(action.activate());require(scope->backend_0.menus==1&&scope->backend_0.actions==1&&scope->backend_0.clicks==1,
                                                        "Generated card mixed menu/action events with main activation");
    scope->backend_0.title.set("Renamed");require(take(surface.action_options()).accessibility_name=="Renamed","Generated card did not update its title");
    auto graph=scope->native_lifetime.lock();scope->backend_0.progress.set(2);
    require(graph&&!take(graph->failures()).empty()&&take(surface.action_options()).accessibility_name=="Renamed","Invalid card input replaced its last good state");
    scope->backend_0.progress.set(.7);require(take(graph->failures()).empty(),"Card did not recover after invalid state");
    scope->backend_0.kind.set(lvui::CardType::file_preview);
    require(!take(menu.interaction()).visible && !take(action.interaction()).visible && take(root.snapshot()).size.height==320,
            "Reactive card type did not update preview layout or hide default actions");
    scope->backend_0.enabled.set(false);good(surface.activate());require(scope->backend_0.clicks==1,"Reactive card disable was ignored");
    scope->backend_0.visible.set(false);require(!take(root.interaction()).visible,"Reactive card visibility was ignored");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!surface.snapshot(),"Generated card retained its owner");tree->close();
}
