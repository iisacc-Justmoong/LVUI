#include "NativeOwnedSegmentedControl.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
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
    auto nested=find(tree,"/nestedButton"),conditional=find(tree,"/conditional"),repeated=find(tree,"/repeated"),inner=find(tree,"/innerButton");
    auto piece=find(tree,"/pieceButton"),help=find(tree,"/nestedHelp");
    auto bounds=take(first.snapshot()).bounds;require(!has_fill(frame,bounds),"Generated group did not force its child tone");
    for(const auto& child:{nested,conditional,repeated,piece,help})require(!has_fill(frame,take(child.snapshot()).bounds),
        "Segmented style was not inherited by a nested activation");
    require(has_fill(frame,take(inner.snapshot()).bounds),"Outer group overrode the nearest inner group");
    scope->backend_0.innerForce.set(true);frame=take(tree->drawing_snapshot({900,260}));
    require(!has_fill(frame,take(inner.snapshot()).bounds),"Inner group did not control its child style");
    scope->backend_0.innerForce.set(false);
    good(first.activate());good(second.activate());good(icon.activate());require(scope->backend_0.clicks==3,"Generated segmented child actions were lost");
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
    scope->backend_0.enabled.set(false);good(first.activate());require(scope->backend_0.clicks==3,"Disabled group allowed child activation");
    good(host.clear_content());scope.reset();graph.reset();require(!first.snapshot(),"Retired generated segmented child remained active");tree->close();
}
