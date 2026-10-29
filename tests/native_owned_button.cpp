#include "NativeOwnedButton.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}
void good(ii::core::Result<void> result){if(!result)throw std::runtime_error(result.error().message);}
bool near(double a,double b){return std::abs(a-b)<1e-4;}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    const auto snapshots=take(state->snapshots());
    const auto found=std::ranges::find_if(snapshots,[&](const auto& item){return item.id.ends_with(suffix);});
    if(found==snapshots.end())throw std::runtime_error("Button did not mount");
    return take(state->find(found->id));
}
ii::gui::Color background(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    const auto find_in=[&](const auto& panels)->std::optional<ii::gui::Color>{
        for(const auto& panel:panels)if(panel.bounds==bounds&&panel.stroke_width==0)return panel.top;
        return std::nullopt;
    };
    if(auto color=find_in(frame.panels))return *color;
    for(const auto& layer:frame.overlays)if(auto color=find_in(layer.panels))return *color;
    throw std::runtime_error("Button background was not drawn");
}
bool has_text(const ii::gui::CanvasFrame& frame,std::string_view text,std::string_view family){
    const auto in=[&](const auto& runs){return std::ranges::any_of(runs,[&](const auto& run){return run.text==text&&run.font_family==family;});};
    if(in(frame.texts))return true;
    return std::ranges::any_of(frame.overlays,[&](const auto& layer){return in(layer.texts);});
}
bool has_icon(const ii::gui::CanvasFrame& frame,const std::shared_ptr<const ii::gui::RasterImage>& image){
    const auto in=[&](const auto& runs){return std::ranges::any_of(runs,[&](const auto& run){return run.image==image&&near(run.bounds.width,16)&&near(run.bounds.height,16);});};
    if(in(frame.images))return true;
    return std::ranges::any_of(frame.overlays,[&](const auto& layer){return in(layer.images);});
}
}
int main(){
    using View=lvui_generated::NativeOwnedButton;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{640,480}));
    auto image=take(ii::gui::rgba_image(1,1,std::vector<std::uint8_t>{255,255,255,255}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder){return View::build(builder,{},[&](auto& value){value.backend_0.icon.set(image);},&scope);}));
    auto button=find(state,"/save"),icon=find(state,"/icon");
    auto before=take(button.snapshot());
    require(before.kind==ii::gui::WidgetKind::action_surface&&before.text=="Save"&&
            before.accessibility_name=="Save document"&&before.enabled,"Independent button semantics or identity are wrong");
    auto frame=take(state->drawing_snapshot({640,480}));
    const auto idle=background(frame,before.bounds);
    require(has_text(frame,"Save",before.font_family)&&has_icon(frame,image),"Button text, resolved font or icon was not painted");
    const auto x=before.bounds.x+before.bounds.width/2,y=before.bounds.y+before.bounds.height/2;
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=x,.y=y}));
    const auto hover=background(take(state->drawing_snapshot({640,480})),before.bounds);
    require(hover!=idle,"Hover did not update the LVUI button palette");
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=x,.y=y}));
    const auto pressed=background(take(state->drawing_snapshot({640,480})),before.bounds);
    require(pressed!=hover,"Press did not update the LVUI button palette");
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=x,.y=y}));
    require(scope->backend_0.saves==1,"Pointer activation did not invoke the action once");
    good(button.focus());
    good(state->input({.kind=ii::gui::WidgetInputKind::key_down,.key=ii::gui::WidgetKey::enter}));
    good(state->input({.kind=ii::gui::WidgetInputKind::key_up,.key=ii::gui::WidgetKey::enter}));
    require(scope->backend_0.saves==2,"Keyboard activation did not invoke the action once");
    scope->backend_0.text.set("Publish");scope->backend_0.height.set(30);
    auto updated=take(button.snapshot());
    require(updated.id==before.id&&updated.text=="Publish"&&near(updated.bounds.height,30),"Reactive update replaced the button or lost its dimensions");
    require(has_text(take(state->drawing_snapshot({640,480})),"Publish",updated.font_family),"Reactive text was not painted");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Button binding graph is unavailable");
    scope->backend_0.height.set(-1);
    scope->backend_0.text.set("Later");
    require(!take(graph->failures()).empty()&&take(button.snapshot()).text=="Publish"&&near(take(button.snapshot()).bounds.height,30),
            "Invalid candidate changed the retained button");
    scope->backend_0.height.set(32);
    require(take(graph->failures()).empty()&&take(button.snapshot()).text=="Later"&&near(take(button.snapshot()).bounds.height,32),
            "Button did not recover after an invalid candidate");
    const std::string long_label(2000,'W');
    scope->backend_0.text.set(long_label);
    const auto long_button=take(button.snapshot());
    require(long_button.text==long_label&&long_button.accessibility_name=="Save document"&&
            has_text(take(state->drawing_snapshot({640,480})),long_label,long_button.font_family),
            "Long button text did not draw with its original accessible name");
    scope->backend_0.enabled.set(false);good(button.activate());
    require(!take(button.snapshot()).enabled&&scope->backend_0.saves==2,"Disabled button admitted activation");
    scope->backend_0.enabled.set(true);scope->backend_0.tone.set(lvui::ButtonTone::disabled);good(button.activate());
    require(!take(button.snapshot()).enabled&&scope->backend_0.saves==2,"Disabled tone admitted activation");
    scope->backend_0.tone.set(lvui::ButtonTone::primary);good(button.activate());
    require(take(button.snapshot()).enabled&&scope->backend_0.saves==3,"Button did not recover its action");
    good(icon.activate());require(scope->backend_0.tools==1,"Icon action was lost");
    std::weak_ptr<View::Scope> weak=scope;
    good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!button.snapshot()&&!icon.snapshot(),"Retired button retained its scope");
    state->close();
}
