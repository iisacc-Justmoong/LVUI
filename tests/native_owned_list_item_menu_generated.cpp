#include "NativeOwnedListItemMenu.generated.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <any>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& item){return item.id.ends_with(suffix);});
    if(node==nodes.end())throw std::runtime_error("Generated menu node is missing");return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemMenu;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto more=find(tree,"/item.actions.slot2");good(more.activate());auto popup=find(tree,"/item.popup");
    require(take(popup.snapshot()).opened,"Generated rich menu did not open");
    good(find(tree,"/option-0").activate());auto child=find(tree,"/submenu-0");
    require(take(child.snapshot()).opened,"Generated submenu did not open");
    auto old=find(tree,"/option-0-0");good(old.activate());
    const auto& selected=scope->backend_0.selections.back();
    require(selected.path==std::vector<std::size_t>({0,0})&&selected.entry&&
            std::any_cast<std::string>(*selected.entry->payload)=="asset-42"&&!take(popup.snapshot()).opened,
            "Generated callback lost the nested path or payload");
    good(more.activate());good(find(tree,"/option-0").activate());
    lvui::ListItemMenuEntry replacement;replacement.label="Publish";
    scope->backend_0.entries.set({replacement});(void)take(find(tree,"/item").snapshot());
    require(!take(popup.snapshot()).opened,"Reactive replacement did not close the old menu");
    good(more.activate());good(find(tree,"/option-0").activate());
    require(scope->backend_0.selections.back().entry->label=="Publish"&&!old.snapshot(),
            "Reactive rich menu replacement did not retire the old generation");
    auto graph=scope->native_lifetime.lock();
    auto bad=std::vector<lvui::ListItemMenuEntry>(401,replacement);scope->backend_0.entries.set(bad);
    require(!take(graph->failures()).empty(),"Invalid reactive rich menu was silently accepted");
    scope->backend_0.entries.set({replacement});require(take(graph->failures()).empty(),"Rich menu did not recover from invalid input");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired(),"Generated rich menu retained its owner");tree->close();
}
