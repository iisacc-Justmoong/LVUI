#include "NativeOwnedListItemDetailControls.generated.h"
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
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view within,std::string_view suffix,std::string_view text={},double minimum_y=-1){
    auto nodes=take(tree->snapshots());auto node=std::ranges::find_if(nodes,[&](const auto& value){return value.id.find(within)!=value.id.npos&&value.id.ends_with(suffix)&&(text.empty()||value.text==text)&&value.bounds.y>=minimum_y;});
    if(node==nodes.end()){
        std::string message="Generated detail control was not mounted: "+std::string(within)+" / "+std::string(suffix);
        for(const auto& value:nodes)message+="\n"+value.id+" text="+value.text+" visible="+std::to_string(value.visible);
        throw std::runtime_error(message);
    }
    return take(tree->find(node->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedListItemDetailControls;using namespace ii::gui;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,360}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    (void)take(tree->drawing_snapshot({500,360}));
    auto unit=find(tree,"","/control","px");good(unit.activate());
    good(find(tree,"","/option-1","cm").activate());
    auto stepper=find(tree,"quantity.stepper","quantity.stepper");good(stepper.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::up}));
    auto first=find(tree,"","/segment-0","Original"),second=find(tree,"","/segment-1","Edited");
    good(first.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(scope->backend_0.events==std::vector<std::string>({"unit:1","quantity:3","segment:1"})&&
            scope->backend_0.unit_index.get()==1&&scope->backend_0.segment_index.get()==1&&take(second.snapshot()).checked,
            "Generated detail control events or reactive echo changed");
    scope->backend_0.arrow.set(lvui::StepperArrow::down);good(stepper.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::up}));
    require(scope->backend_0.quantity.get()==3,"Reactive down-only stepper accepted an increment");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(scope->backend_0.quantity.get()==2&&scope->backend_0.events.back()=="quantity:2","Reactive stepper arrow did not update");
    scope->backend_0.tone.set(lvui::ButtonTone::disabled);
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(scope->backend_0.quantity.get()==2,"Reactive disabled stepper changed its value");
    (void)find(tree,"","/control","Automatic",100);
    scope->backend_0.kind.set(lvui::ListItemVariant::detail_quantity);(void)take(tree->drawing_snapshot({500,360}));
    (void)find(tree,"","/control","Format",100);
    scope->backend_0.kind.set(lvui::ListItemVariant::detail_settings);(void)take(tree->drawing_snapshot({500,360}));
    scope->backend_0.show_segments.set(false);
    require(!take(second.interaction()).visible,"Generated segment visibility did not update");
    scope->backend_0.show_segments.set(true);scope->backend_0.segments.set({"First","Second","Third"});
    auto replacement=find(tree,"","/segment-2","Third");good(replacement.activate());
    require(scope->backend_0.events.back()=="segment:2"&&scope->backend_0.segment_index.get()==2,
            "Generated dynamic segments failed to replace and select an option");
    auto graph=scope->native_lifetime.lock();std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired(),"Generated detail controls retained their owner");tree->close();
}
