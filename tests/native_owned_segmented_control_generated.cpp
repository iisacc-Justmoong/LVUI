#include "NativeOwnedSegmentedControl.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool has_fill(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    for(const auto& panel:frame.panels)if(panel.bounds==bounds&&panel.stroke_width==0)return true;
    for(const auto& layer:frame.overlays)for(const auto& panel:layer.panels)if(panel.bounds==bounds&&panel.stroke_width==0)return true;
    return false;
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto found=std::ranges::find_if(nodes,[&](const auto& node){return node.id.ends_with(suffix);});
    if(found==nodes.end())throw std::runtime_error("Generated segmented child was not found");return take(tree->find(found->id));
}
}
int main(){
    using View=lvui_generated::NativeOwnedSegmentedControl;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{900,260}));std::shared_ptr<View::Scope> scope;
    auto image=take(rgba_image(1,1,std::vector<std::uint8_t>{255,255,255,255}));
    good(host.set_content([&](auto& builder){return View::build(builder,{},[&](auto& value){value.backend_0.icon.set(image);},&scope);}));
    auto frame=take(tree->drawing_snapshot({900,260}));auto first=find(tree,"/first"),second=find(tree,"/second"),icon=find(tree,"/icon");
    const auto group_bounds=take(find(tree,"/group").snapshot()).bounds;
    const auto icons_bounds=take(find(tree,"/icons").snapshot()).bounds;
    const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    const auto surface=lvui::figma_defaults::panel_sub_surface;
    const auto matches=[&](const ii::gui::Rect& rect){
        const auto contains=[&](const auto& panels){return std::ranges::any_of(panels,[&](const auto& p){
            return p.bounds==rect && p.stroke_width==0 && p.radius==8 && std::abs(p.top.red-linear(surface.red))<1e-5 &&
                   std::abs(p.top.green-linear(surface.green))<1e-5 && std::abs(p.top.blue-linear(surface.blue))<1e-5;});};
        if(contains(frame.panels))return true;for(const auto& layer:frame.overlays)if(contains(layer.panels))return true;return false;
    };
    const auto fill_count=[&](const ii::gui::Rect& rect){
        std::size_t count=0;
        const auto add=[&](const auto& panels){for(const auto& p:panels)
            if(p.bounds==rect&&p.stroke_width==0)++count;};
        add(frame.panels);for(const auto& layer:frame.overlays)add(layer.panels);return count;
    };
    require(matches(group_bounds)&&matches(icons_bounds),"Generated segments did not use the Figma surface color");
    require(icons_bounds.height==22,"Generated icon segment did not use the Figma intrinsic height");
    require(take(icon.snapshot()).bounds==icons_bounds,"Generated icon segment does not fit its button");
    bool border=false;const auto inspect_border=[&](const auto& panels){for(const auto& panel:panels)
        if((panel.bounds==group_bounds||panel.bounds==icons_bounds)&&panel.stroke_width>0)border=true;};
    inspect_border(frame.panels);for(const auto& layer:frame.overlays)inspect_border(layer.panels);
    require(!border,"Generated segment surface drew a border");
    auto nested=find(tree,"/nestedButton"),conditional=find(tree,"/conditional"),repeated=find(tree,"/repeated"),inner=find(tree,"/innerButton");
    auto piece=find(tree,"/pieceButton"),help=find(tree,"/nestedHelp");
    auto bounds=take(first.snapshot()).bounds;require(!has_fill(frame,bounds),"Generated group did not force its child tone");
    for(const auto& child:{nested,conditional,repeated,piece,help})require(!has_fill(frame,take(child.snapshot()).bounds),
        "Segmented style was not inherited by a nested activation");
    const auto inner_bounds=take(inner.snapshot()).bounds;
    const auto inner_fills=fill_count(inner_bounds);
    require(inner_fills>=2,"Outer group overrode the nearest inner group");
    scope->backend_0.innerForce.set(true);frame=take(tree->drawing_snapshot({900,260}));
    require(fill_count(take(inner.snapshot()).bounds)+1==inner_fills,"Inner group did not control its child style");
    scope->backend_0.innerForce.set(false);
    good(first.activate());good(second.activate());good(icon.activate());require(scope->backend_0.clicks==3,"Generated segmented child actions were lost");
    require(scope->backend_0.method_calls==0,"Child activation incorrectly invoked segmented methods");
    scope->backend_0.methods={[&](const auto&){++scope->backend_0.method_calls;}};scope->publish_native_changes();
    good(inner.activate());require(scope->backend_0.method_calls==0&&scope->backend_0.clicks==4,
        "Reactive segmented methods ran on child activation");
    scope->backend_0.tone.set(lvui::ButtonTone::destructive);frame=take(tree->drawing_snapshot({900,260}));
    require(!has_fill(frame,bounds),"Reactive child tone replaced the group override");
    scope->backend_0.force.set(false);frame=take(tree->drawing_snapshot({900,260}));
    require(has_fill(frame,bounds),"Releasing the override did not restore the reactive child tone");
    for(const auto& child:{nested,conditional,repeated,piece,help})require(has_fill(frame,take(child.snapshot()).bounds),
        "Nested child did not restore its authored tone");
    scope->backend_0.force.set(true);frame=take(tree->drawing_snapshot({900,260}));
    require(!has_fill(frame,bounds),"Reapplying group tone failed");
    scope->backend_0.show.set(false);require(!conditional.snapshot(),"Removed conditional child remained active");
    scope->backend_0.show.set(true);conditional=find(tree,"/conditional");
    scope->backend_0.items.set(std::vector<NativeOwnedSegmentedItem>{{"b","Replacement"}});
    require(!repeated.snapshot(),"Removed repeated child remained active");repeated=find(tree,"/repeated");
    frame=take(tree->drawing_snapshot({900,260}));
    for(const auto& child:{conditional,repeated})require(!has_fill(frame,take(child.snapshot()).bounds),
        "Replacement child did not inherit the current segmented style");
    auto graph=scope->native_lifetime.lock();scope->backend_0.spacing.set(-1);
    require(!take(graph->failures()).empty(),"Invalid reactive segmented option was accepted");
    scope->backend_0.spacing.set(3);require(take(graph->failures()).empty(),"Segmented control did not recover from invalid options");
    scope->backend_0.enabled.set(false);good(first.activate());require(scope->backend_0.clicks==4,"Disabled group allowed child activation");
    good(host.clear_content());scope.reset();graph.reset();require(!first.snapshot(),"Retired generated segmented child remained active");tree->close();
}
