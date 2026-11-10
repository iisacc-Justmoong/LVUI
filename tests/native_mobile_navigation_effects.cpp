#include <LVUI/NativeMobileNavigation.h>
#include <iiGUI/NativeMobileNavigationView.h>
#include <iiGUI/MobileNavigationModel.h>
#include <iiGUI/MobileNavigationMotion.h>
#include <iiGUI/NativeMaterialView.h>
#include <iiGUI/SvgImageCache.h>
using MobileNavTestModel=lvui::MobileNavigationModelBinding<ii::gui::BasicMobileNavigationModel>;
using MobileNavTestMotion=lvui::MobileNavigationMotionBinding<ii::gui::BasicMobileNavigationMotion>;
using MobileNavTestMaterial=lvui::native::MaterialBinding<ii::gui::BasicMaterial,ii::gui::BasicMaterialGradient,ii::gui::BasicNativeMaterial,ii::gui::Widget>;
using MobileNavTestView=lvui::native::MobileNavigationBinding<ii::gui::BasicNativeMobileNavigation,MobileNavTestModel,MobileNavTestMotion,MobileNavTestMaterial,ii::gui::CachedSvgDecoder>;
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace {
void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    auto nodes=take(state->snapshots());auto it=std::ranges::find_if(nodes,[&](auto& n){return n.id.ends_with(suffix);});
    if(it==nodes.end())throw std::runtime_error("Mobile navigation child missing");return take(state->find(it->id));
}
bool text(const ii::gui::CanvasFrame& f,std::string_view value){
    auto has=[&](auto& runs){return std::ranges::any_of(runs,[&](auto& r){return r.text==value;});};
    return has(f.texts)||std::ranges::any_of(f.overlays,[&](auto& l){return has(l.texts);});
}
bool backdrop(const ii::gui::CanvasFrame& f){return std::ranges::any_of(f.overlays,[](auto& l){return l.backdrop&&
    l.backdrop->capture&&l.backdrop->source==ii::gui::BackdropSource::previous_layers;});}
double indicator_width(const ii::gui::CanvasFrame& f,const ii::gui::Rect& body){
    auto token=lvui::figma_defaults::panel_surface_input;const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    const auto matches=[&](const ii::gui::Panel& p){return std::abs(p.top.red-linear(token.red))<1e-4&&
        std::abs(p.top.green-linear(token.green))<1e-4&&std::abs(p.top.blue-linear(token.blue))<1e-4&&
        p.bounds.y>=body.y&&p.bounds.y+p.bounds.height<=body.y+body.height+.01&&p.stroke_width==0;};
    for(auto& p:f.panels)if(matches(p))return p.bounds.width;
    for(auto& l:f.overlays)for(auto& p:l.panels)if(matches(p))return p.bounds.width;
    throw std::runtime_error("Selected indicator was not painted");
}
}
int main(){
    using namespace ii::gui;using namespace std::chrono_literals;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);auto motion=std::make_shared<MotionPreferences>();
    detail::WidgetFrameHooks hooks;hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};hooks.preferences=motion;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"host",{402,120}));std::optional<MobileNavTestView> navigation;
    lvui::MobileNavigationConfiguration options;options.platform=lvui::MobileNavigationPlatform::ios;options.material_enabled=true;
    options.current_index=0;options.auto_select=false;
    options.items={{"home","Home","home","Home"},{"tools","Tools","collection-asset","Tools"},{"notice","Notice","toolwindownotifications","Notice"}};
    options.items[1].badge="37";options.items[1].badge_accessibility_name="37 notifications";options.items[2].badge_dot=true;
    int activations=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto result=MobileNavTestView::create(builder,{},"nav",options,{.activated=[&](int){++activations;}});
        if(!result)return std::unexpected(result.error());navigation=*result;return {};}));
    auto frame=take(tree->drawing_snapshot({402,120}));auto body=find(tree,"/nav.body"),tab=find(tree,"/nav.tab1");
    require(backdrop(frame),"iOS material did not capture the previous layers");
    require(text(frame,"37")&&take(tab.snapshot()).accessibility_description=="37 notifications","Badge ink or accessible description missing");
    require(take(find(tree,"/nav.badge2").snapshot()).visible,"Dot badge missing");
    const auto badge=take(find(tree,"/nav.badge1").snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=badge.x+badge.width/2,.y=badge.y+badge.height/2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=badge.x+badge.width/2,.y=badge.y+badge.height/2}));
    require(activations==1,"Badge intercepted pointer input intended for its tab");
    const auto body_bounds=take(body.snapshot()).bounds;require(indicator_width(frame,body_bounds)==56,"Initial selection surface incorrect");
    const auto start=ii::runtime::Clock::now();options.current_index=2;good(navigation->set_options(options));
    (void)frames.step(start+140ms);frame=take(tree->drawing_snapshot({402,120}));
    require(indicator_width(frame,body_bounds)>56,"Selection did not stretch while moving");
    (void)frames.step(start+500ms);frame=take(tree->drawing_snapshot({402,120}));
    require(std::abs(indicator_width(frame,body_bounds)-56)<.01,"Selection did not settle");
    const auto settled=frames.stats().callbacks;(void)frames.step(start+600ms);
    require(frames.stats().callbacks==settled,"Settled navigation continued to request frames");
    good(motion->set_reduced_motion(true));options.current_index=0;good(navigation->set_options(options));
    require(std::abs(indicator_width(take(tree->drawing_snapshot({402,120})),body_bounds)-56)<.01,"Reduced motion failed");
    options.items[1].badge.clear();options.items[1].badge_accessibility_name.clear();options.items[2].badge_dot=false;
    good(navigation->set_options(options));frame=take(tree->drawing_snapshot({402,120}));
    require(!text(frame,"37")&&!take(find(tree,"/nav.badge2").snapshot()).visible,"Removed badge remained visible");
    options.platform=lvui::MobileNavigationPlatform::android;good(navigation->set_options(options));
    require(!backdrop(take(tree->drawing_snapshot({402,120}))),"Android navigation unexpectedly captured a material backdrop");
    good(tab.activate());require(activations==2,"Tab activation failed after its badge was removed");
    good(motion->set_reduced_motion(false));options.current_index=2;good(navigation->set_options(options));
    options.enabled=false;good(navigation->set_options(options));const auto disabled=frames.stats().callbacks;
    (void)frames.step(start+700ms);require(frames.stats().callbacks==disabled,"Disabled navigation continued requesting frames");
    options.enabled=true;options.current_index=0;good(navigation->set_options(options));good(navigation->root().set_visible(false));
    (void)frames.step(start+800ms);const auto hidden=frames.stats().callbacks;(void)frames.step(start+900ms);
    require(frames.stats().callbacks==hidden,"Hidden navigation continued requesting frames");
    good(host.clear_content());require(!navigation->set_options(options),"Retired navigation remained writable");
    (void)frames.step(start+1s);tree->close();
}
