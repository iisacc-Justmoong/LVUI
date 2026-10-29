#include "NativeOwnedDropdownButton.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Generated Dropdown was not found");return take(tree->find(found->id));
}
bool has_fill(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    for(const auto& panel:frame.panels)if(panel.bounds==bounds&&panel.stroke_width==0)return true;
    for(const auto& layer:frame.overlays)for(const auto& panel:layer.panels)if(panel.bounds==bounds&&panel.stroke_width==0)return true;
    return false;
}
bool has_image(const ii::gui::CanvasFrame& frame,const std::shared_ptr<const ii::gui::RasterImage>& image){
    for(const auto& run:frame.images)if(run.image==image)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.images)if(run.image==image)return true;
    return false;
}
}
int main(){
    using View=lvui_generated::NativeOwnedDropdownButton;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,220}));std::shared_ptr<View::Scope> scope;
    auto image=take(rgba_image(1,1,std::vector<std::uint8_t>{255,255,255,255}));
    good(host.set_content([&](auto& builder){return View::build(builder,{},[&](auto& value){value.backend_0.icon.set(image);},&scope);}));
    auto frame=take(tree->drawing_snapshot({400,220}));auto drop=find(tree,"/drop"),label=find(tree,"/label"),icon=find(tree,"/icon"),contained=find(tree,"/contained");
    require(take(drop.snapshot()).bounds.width==100&&take(icon.snapshot()).accessibility_name=="Menu"&&
        !has_fill(frame,take(contained.snapshot()).bounds),"Generated Dropdown defaults or segmented tone changed");
    auto b=take(drop.snapshot()).bounds;good(tree->input({.kind=WidgetInputKind::pointer_down,.x=b.x+5,.y=b.y+5}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=b.x+5,.y=b.y+5}));
    require(scope->backend_0.clicks==1&&scope->backend_0.presses==1&&scope->backend_0.releases==1,"Generated Dropdown callbacks failed");
    good(label.activate());good(icon.activate());good(contained.activate());require(scope->backend_0.clicks==4,"Dropdown variants lost actions");
    scope->backend_0.iconMode.set(true);scope->backend_0.width.set(120);frame=take(tree->drawing_snapshot({400,220}));
    require(take(drop.snapshot()).bounds.width==120&&has_image(frame,image),"Reactive Dropdown icon or width was not applied");
    scope->backend_0.force.set(false);frame=take(tree->drawing_snapshot({400,220}));
    require(has_fill(frame,take(contained.snapshot()).bounds),"Dropdown did not restore its authored tone");
    auto graph=scope->native_lifetime.lock();scope->backend_0.width.set(0);
    require(!take(graph->failures()).empty()&&take(drop.snapshot()).bounds.width==120,"Invalid Dropdown width replaced the live state");
    scope->backend_0.width.set(100);require(take(graph->failures()).empty(),"Dropdown did not recover after invalid input");
    scope->backend_0.enabled.set(false);good(drop.activate());require(scope->backend_0.clicks==4,"Disabled Dropdown accepted activation");
    good(host.clear_content());scope.reset();graph.reset();require(!drop.snapshot(),"Retired generated Dropdown remained active");tree->close();
}
