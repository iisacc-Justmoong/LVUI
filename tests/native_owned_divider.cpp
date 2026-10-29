#include "NativeOwnedDivider.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
bool near(double a,double b){return std::abs(a-b)<1e-4;}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    const auto snapshots=take(state->snapshots());
    const auto found=std::ranges::find_if(snapshots,[&](const auto& item){return item.id.ends_with(suffix);});
    if(found==snapshots.end())throw std::runtime_error("Divider did not mount");
    return take(state->find(found->id));
}
bool line(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds,ii::gui::Color color){
    const auto in=[&](const auto& panels){return std::ranges::any_of(panels,[&](const auto& panel){return
        near(panel.bounds.x,bounds.x)&&near(panel.bounds.y,bounds.y)&&near(panel.bounds.width,bounds.width)&&
        near(panel.bounds.height,bounds.height)&&near(panel.top.red,color.red)&&near(panel.top.green,color.green)&&
        near(panel.top.blue,color.blue)&&near(panel.top.alpha,color.alpha);});};
    if(in(frame.panels))return true;
    return std::ranges::any_of(frame.overlays,[&](const auto& layer){return in(layer.panels);});
}
}
int main(){
    using View=lvui_generated::NativeOwnedDivider;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{640,480}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
    auto rule=find(state,"/rule"),context=find(state,"/context");
    const auto first=take(rule.snapshot()),second=take(context.snapshot());
    require(first.kind==ii::gui::WidgetKind::absolute_stack&&near(first.bounds.width,120)&&near(first.bounds.height,6)&&
            near(second.bounds.width,145)&&near(second.bounds.height,3),"Divider natural size or kind is incorrect");
    auto frame=take(state->drawing_snapshot({640,480}));
    require(line(frame,{first.bounds.x+4,first.bounds.y+2,112,2},{1,0,0,.5})&&
            line(frame,{second.bounds.x+4,second.bounds.y+1,137,1},{1,1,1,.3}),"Divider drawing or default context inset is incorrect");
    require(!rule.activate(),"Divider unexpectedly accepted activation");
    scope->backend_0.axis.set(std::string("vertical"));scope->backend_0.length.set(80);
    auto updated=take(rule.snapshot());
    require(updated.id==first.id&&near(updated.bounds.width,6)&&near(updated.bounds.height,80),"Reactive axis update replaced or mis-sized the divider");
    require(line(take(state->drawing_snapshot({640,480})),{updated.bounds.x+2,updated.bounds.y+4,2,72},{1,0,0,.5}),"Vertical line was not drawn");
    scope->backend_0.color.set({.5,.25,.75,.75});
    require(line(take(state->drawing_snapshot({640,480})),{updated.bounds.x+2,updated.bounds.y+4,2,72},
                 {.21404114,.05087609,.52252155,.75}),"Reactive color was not converted to linear light");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Divider binding graph unavailable");
    scope->backend_0.thickness.set(-1);scope->backend_0.length.set(90);
    require(!take(graph->failures()).empty()&&near(take(rule.snapshot()).bounds.height,80),"Invalid update changed the divider");
    scope->backend_0.thickness.set(1);
    require(take(graph->failures()).empty()&&near(take(rule.snapshot()).bounds.height,90),"Divider did not recover after invalid metrics");
    scope->backend_0.axis.set(std::string("diagonal"));
    require(!take(graph->failures()).empty()&&near(take(rule.snapshot()).bounds.height,90),"Invalid axis changed the divider");
    scope->backend_0.axis.set(std::string("horizontal"));
    require(take(graph->failures()).empty()&&near(take(rule.snapshot()).bounds.width,90),"Divider did not recover after invalid axis");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!rule.snapshot()&&!context.snapshot(),"Retired divider retained its scope");
    state->close();
}
