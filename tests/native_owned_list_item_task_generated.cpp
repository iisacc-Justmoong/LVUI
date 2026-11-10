#include "NativeOwnedListItemTask.generated.h"
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
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix,std::string_view text={},double minimum_y=-1){
    auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix)&&(text.empty()||value.text==text)&&value.bounds.y>=minimum_y;});
    if(node==nodes.end()){
        std::string message="Generated task control was not mounted: "+std::string(suffix)+" / "+std::string(text);
        for(const auto& value:nodes)message+="\n"+value.id+" text="+value.text+" y="+std::to_string(value.bounds.y)+" visible="+std::to_string(value.visible);
        throw std::runtime_error(message);
    }
    return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemTask;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,400}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    (void)take(tree->drawing_snapshot({500,400}));
    good(find(tree,"task.checkbox").activate());auto selector=find(tree,"/control","Low");good(selector.activate());
    good(find(tree,"/option-1","High").activate());auto stepper=find(tree,"task.stepper");good(stepper.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::up}));
    require(scope->backend_0.checked.get()&&scope->backend_0.priority.get()==1&&scope->backend_0.quantity.get()==3&&
        scope->backend_0.events==std::vector<std::string>({"checked:true","priority:1","quantity:3"}),
        "Generated task did not publish reactive values");
    (void)find(tree,"/control","Automatic",100);
    scope->backend_0.kind.set(lvui::ListItemVariant::task);(void)take(tree->drawing_snapshot({500,400}));
    (void)find(tree,"/control","Priority",100);
    require(!take(find(tree,"dynamic.checkbox").snapshot()).checked,"A dynamically selected task did not start unchecked");
    (void)find(tree,".actions.slot1","Done",100);(void)find(tree,".actions.slot0","Assign",100);
    scope->backend_0.kind.set(lvui::ListItemVariant::detail_settings);(void)take(tree->drawing_snapshot({500,400}));
    (void)find(tree,"/control","Automatic",100);
    require(take(find(tree,"dynamic.toggle").snapshot()).checked,"Returning to settings did not restore its default toggle");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired(),"Generated task retained its owner");tree->close();
}
