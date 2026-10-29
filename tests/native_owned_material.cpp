#include "NativeOwnedMaterial.generated.h"
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
bool near(double a,double b){return std::abs(a-b)<1e-4;}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    const auto snapshots=take(state->snapshots());const auto found=std::ranges::find_if(snapshots,[&](const auto& item){return item.id.ends_with(suffix);});
    if(found==snapshots.end())throw std::runtime_error("Material widget missing");return take(state->find(found->id));
}
const ii::gui::CanvasLayer& surface(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    const auto found=std::ranges::find_if(frame.overlays,[&](const auto& layer){return layer.backdrop&&near(layer.backdrop->bounds.x,bounds.x)&&
        near(layer.backdrop->bounds.y,bounds.y)&&near(layer.backdrop->bounds.width,bounds.width)&&near(layer.backdrop->bounds.height,bounds.height);});
    if(found==frame.overlays.end())throw std::runtime_error("Material drawing missing");return *found;
}
void generated(){
    using View=lvui_generated::NativeOwnedMaterial;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task work){work();});
    auto host=take(state->content_host({},"host",{640,480}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
    auto panel=find(state,"/panel"),child=find(state,"/child"),window=find(state,"/window"),plain=find(state,"/surface");
    auto bounds=take(panel.snapshot()).bounds;auto frame=take(state->drawing_snapshot({640,480}));auto layer=surface(frame,bounds);
    require(near(bounds.width,120)&&near(bounds.height,80)&&layer.backdrop->underlay&&layer.shadow&&
        layer.backdrop->capture&&layer.backdrop->source==ii::gui::BackdropSource::previous_layers&&
        near(layer.backdrop->tint.alpha,.25)&&near(layer.backdrop->blur_radius,16)&&near(layer.backdrop->underlay_tint->red,1),
        "Panel composition lost authored style or backdrop");
    const auto cached=layer.backdrop->underlay;
    frame=take(state->drawing_snapshot({640,480}));require(surface(frame,bounds).backdrop->underlay==cached,"Unchanged material lost its retained gradient");
    require(!surface(frame,take(plain.snapshot()).bounds).backdrop->underlay,"Zero-strength radial texture was generated");
    require(near(surface(frame,take(window.snapshot()).bounds).backdrop->blur_radius,64),"Window density default changed");
    require(take(child.snapshot()).bounds.width>0,"Material did not retain its child");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Material binding graph missing");
    scope->backend_0.density.set(lvui::MaterialDensity::dense75);scope->backend_0.accent.set({0,1,0,1});
    frame=take(state->drawing_snapshot({640,480}));layer=surface(frame,bounds);
    require(near(layer.backdrop->tint.alpha,.75)&&near(layer.backdrop->border.alpha,.12)&&near(layer.backdrop->underlay_tint->green,1),
        "Reactive density or accent did not update the material");
    scope->backend_0.accent.set({0,1,0,.5});frame=take(state->drawing_snapshot({640,480}));layer=surface(frame,bounds);
    require(layer.backdrop->underlay&&layer.backdrop->underlay!=cached&&near(layer.backdrop->underlay_tint->alpha,1)&&
        layer.backdrop->underlay->rgba()[3]<cached->rgba()[3],"Overlapping gradients lost accent alpha");
    scope->backend_0.outline.set("M0 0 L120 0 L120 60 L60 60 L50 80 L40 60 L0 60 Z");
    frame=take(state->drawing_snapshot({640,480}));layer=surface(frame,bounds);
    require(layer.backdrop->mask&&layer.shadow->mask==layer.backdrop->mask&&layer.backdrop->border_width==0&&
        !layer.images.empty(),"Custom outline did not reach the material, shadow and border");
    const auto mask=layer.backdrop->mask;
    const auto alpha=[&](unsigned x,unsigned y){const auto px=std::min(mask->width()-1,unsigned(double(x)*mask->width()/120));
        const auto py=std::min(mask->height()-1,unsigned(double(y)*mask->height()/80));return mask->rgba()[(std::size_t(py)*mask->width()+px)*4+3];};
    require(alpha(50,70)>200&&alpha(5,75)<20,"SVG silhouette pixels do not match the authored path");
    require(surface(take(state->drawing_snapshot({640,480})),bounds).backdrop->mask==mask,"Unchanged outline was not retained");
    scope->backend_0.outline.set("M0");
    require(!take(graph->failures()).empty()&&surface(take(state->drawing_snapshot({640,480})),bounds).backdrop->mask==mask,
        "Invalid outline changed the live silhouette");
    scope->backend_0.outline.set("");require(take(graph->failures()).empty(),"Valid outline update did not recover");
    scope->backend_0.blur.set(65);
    require(!take(graph->failures()).empty()&&near(surface(take(state->drawing_snapshot({640,480})),bounds).backdrop->blur_radius,16),
        "Invalid blur changed the live material");
    scope->backend_0.blur.set(32);scope->backend_0.width.set(140);bounds=take(panel.snapshot()).bounds;
    frame=take(state->drawing_snapshot({640,480}));layer=surface(frame,bounds);
    require(take(graph->failures()).empty()&&near(bounds.width,140)&&near(layer.backdrop->blur_radius,32)&&layer.backdrop->underlay!=cached,
        "Material did not recover or resize its gradient");
    scope->backend_0.enabled.set(false);require(!take(panel.snapshot()).enabled,"Material did not update child enabled state");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!panel.snapshot(),"Retired material retained its scope");state->close();
}
void lifecycle_and_motion(){
    using namespace std::chrono_literals;ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task work){work();},ii::gui::WidgetAppearance{},std::move(hooks));
    auto host=take(state->content_host({},"host",{200,100}));std::optional<lvui::native::NativeMaterial> material;
    lvui::MaterialConfiguration options;options.width=100;options.height=60;options.primary_color=lvui::StyleColor{0,0,0,1};
    options.motion={.duration=200ms,.curve=lvui::MotionCurve::linear};
    good(host.set_content([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{auto created=lvui::native::NativeMaterial::create(builder,{},"material",options);
        if(!created)return std::unexpected(created.error());material=*created;return {};}));
    auto widget=material->root();options.primary_color=lvui::StyleColor{1,0,0,1};good(material->set_options(options));
    const auto anchor=ii::runtime::Clock::now();(void)frames.step(anchor+100ms);
    auto frame=take(state->drawing_snapshot({200,100}));auto layer=surface(frame,take(widget.snapshot()).bounds);
    require(layer.backdrop->underlay_tint->red>.19&&layer.backdrop->underlay_tint->red<.24,"Scheduled material transition did not reach its midpoint");
    (void)frames.step(anchor+250ms);frame=take(state->drawing_snapshot({200,100}));
    require(near(surface(frame,take(widget.snapshot()).bounds).backdrop->underlay_tint->red,1),"Material animation did not finish");
    bool rejected=false;std::thread worker([&]{auto changed=material->set_options(options);rejected=!changed&&changed.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread changed material");good(host.clear_content());
    auto closed=material->set_options(options);require(!closed&&closed.error().code==ii::core::ErrorCode::closed,"Retired material remained writable");
    material.reset();state->close();
}
}
int main(){generated();lifecycle_and_motion();}
