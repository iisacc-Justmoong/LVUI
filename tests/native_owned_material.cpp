#include "NativeOwnedMaterial.generated.h"
#include <LVUI/NativeMaterialGradient.h>
#include <LVUI/NativeMaterial.h>
#include <iiGUI/NativeMaterialView.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

namespace lvui::native {
using MaterialModel=ii::gui::BasicMaterial<MaterialModelPolicy>;
using NativeMaterialGradient=MaterialGradientBinding<ii::gui::BasicMaterialGradient,MaterialModel>;
using NativeMaterial=MaterialBinding<ii::gui::BasicMaterial,ii::gui::BasicMaterialGradient,ii::gui::BasicNativeMaterial,ii::gui::Widget>;
}
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
std::array<unsigned,4> radial_pixel(const ii::gui::CanvasLayer& layer,double x,double y){
    const auto& material=*layer.backdrop;const auto image=material.underlay;const auto extent=material.underlay_bounds;
    if(!image||extent.width<=0||extent.height<=0)throw std::runtime_error("Material radial image missing");
    const auto px=std::min(image->width()-1,unsigned(std::max(0.,(x-extent.x)*image->width()/extent.width)));
    const auto py=std::min(image->height()-1,unsigned(std::max(0.,(y-extent.y)*image->height()/extent.height)));
    const auto at=(std::size_t(py)*image->width()+px)*4;const auto rgba=image->rgba();
    return {rgba[at],rgba[at+1],rgba[at+2],rgba[at+3]};
}
double linear(double value){return value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);}
void flat_surface(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds,lvui::StyleColor color){
    const auto found=std::ranges::find_if(frame.overlays,[&](const auto& layer){
        return !layer.backdrop&&!layer.shadow&&std::ranges::any_of(layer.panels,[&](const auto& panel){
            const auto match=[&](const auto& value){return near(value.red,linear(color.red))&&near(value.green,linear(color.green))&&
                near(value.blue,linear(color.blue))&&near(value.alpha,1);};
            return panel.bounds==bounds&&near(panel.radius,0)&&near(panel.stroke_width,0)&&match(panel.top)&&match(panel.bottom);
        });
    });
    if(found==frame.overlays.end()){
        std::cerr<<"Expected flat material bounds "<<bounds.x<<','<<bounds.y<<' '<<bounds.width<<'x'<<bounds.height<<'\n';
        for(const auto& layer:frame.overlays){
            if(layer.backdrop)std::cerr<<"  backdrop "<<layer.backdrop->bounds.x<<','<<layer.backdrop->bounds.y<<' '<<layer.backdrop->bounds.width<<'x'<<layer.backdrop->bounds.height<<'\n';
            for(const auto& panel:layer.panels)std::cerr<<"  panel "<<panel.bounds.x<<','<<panel.bounds.y<<' '<<panel.bounds.width<<'x'<<panel.bounds.height<<
                " color "<<panel.top.red<<','<<panel.top.green<<','<<panel.top.blue<<','<<panel.top.alpha<<" effects "<<bool(layer.backdrop)<<','<<bool(layer.shadow)<<'\n';
        }
    }
    require(found!=frame.overlays.end(),"Opaque rectangular material did not produce a flat linear-color fill");
}
void flat_material(){
    using namespace std::chrono_literals;ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);
    ii::gui::detail::WidgetFrameHooks hooks;hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task work){work();},ii::gui::WidgetAppearance{},std::move(hooks));
    auto host=take(state->content_host({},"host",{200,100}));std::optional<lvui::native::NativeMaterial> material;
    lvui::MaterialConfiguration options;options.kind=lvui::MaterialKind::window;
    options.color=lvui::StyleColor{.25,.5,.75,1};options.tint_opacity=1;options.blur_radius=0;options.radius=0;
    options.inner_highlight_opacity=0;options.border_width=0;options.intense_opacity=0;options.faint_opacity=0;
    options.shadow_opacity=0;options.width=100;options.height=60;options.motion_enabled=false;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto created=lvui::native::NativeMaterial::create(builder,{},"material",options);
        if(!created)return std::unexpected(created.error());material=*created;return {};
    }));
    const auto widget=material->root();auto frame=take(state->drawing_snapshot({200,100}));
    auto bounds=take(widget.snapshot()).bounds;flat_surface(frame,bounds,*options.color);
    options.tint_opacity=.5;good(material->set_options(options));
    frame=take(state->drawing_snapshot({200,100}));require(near(surface(frame,bounds).backdrop->tint.alpha,.5),"Transparent material lost its compositing");
    options.tint_opacity=1;options.backdrop=lvui::MaterialBackdrop::previous_layers;good(material->set_options(options));
    frame=take(state->drawing_snapshot({200,100}));require(surface(frame,bounds).backdrop->capture&&
        surface(frame,bounds).backdrop->source==ii::gui::BackdropSource::previous_layers,"Explicit backdrop capture was bypassed");
    options.backdrop=lvui::MaterialBackdrop::none;options.border_width=1;good(material->set_options(options));
    frame=take(state->drawing_snapshot({200,100}));require(near(surface(frame,bounds).backdrop->border_width,1),"Material border was bypassed");
    options.border_width=0;options.blur_radius=8;good(material->set_options(options));
    frame=take(state->drawing_snapshot({200,100}));require(near(surface(frame,bounds).backdrop->blur_radius,8),"Material blur was bypassed");
    options.blur_radius=0;options.backdrop_base_color=lvui::StyleColor{0,0,0,.5};good(material->set_options(options));
    frame=take(state->drawing_snapshot({200,100}));require(near(surface(frame,bounds).backdrop->base.alpha,.5),"Material base was bypassed");
    options.backdrop_base_color.reset();options.intense_opacity=.2;good(material->set_options(options));
    frame=take(state->drawing_snapshot({200,100}));require(bool(surface(frame,bounds).backdrop->underlay),"Material gradient was bypassed");
    options.intense_opacity=0;options.inner_highlight_opacity=.1;good(material->set_options(options));
    frame=take(state->drawing_snapshot({200,100}));require(near(surface(frame,bounds).backdrop->inner_highlight_opacity,.1),"Material highlight was bypassed");
    options.inner_highlight_opacity=0;options.shadow_opacity=.2;good(material->set_options(options));
    frame=take(state->drawing_snapshot({200,100}));require(bool(surface(frame,bounds).shadow),"Material shadow was bypassed");
    options.shadow_opacity=0;options.radius=4;good(material->set_options(options));
    frame=take(state->drawing_snapshot({200,100}));require(near(surface(frame,bounds).backdrop->radius,4),"Rounded material was bypassed");
    options.radius=0;options.outline_path="M0 0 H100 V60 H0 Z";good(material->set_options(options));
    frame=take(state->drawing_snapshot({200,100}));require(bool(surface(frame,bounds).backdrop->mask),"Custom material outline was bypassed");
    options.outline_path.clear();options.color=lvui::StyleColor{.75,.25,.5,1};good(material->set_options(options));
    good(host.set_extent({240,120}));
    frame=take(state->drawing_snapshot({240,120}));bounds=take(widget.snapshot()).bounds;
    require(near(bounds.width,240)&&near(bounds.height,120),"Flat material did not resize");
    flat_surface(frame,bounds,*options.color);
    options.motion_enabled=true;options.motion={.duration=200ms,.curve=lvui::MotionCurve::linear};options.tint_opacity=.5;
    good(material->set_options(options));const auto anchor=ii::runtime::Clock::now();(void)frames.step(anchor+100ms);
    frame=take(state->drawing_snapshot({240,120}));const auto alpha=surface(frame,bounds).backdrop->tint.alpha;
    require(alpha>.5&&alpha<1,"Animated tint did not switch to the translucent material path");
    options.tint_opacity=1;good(material->set_options(options));(void)frames.step(anchor+350ms);
    frame=take(state->drawing_snapshot({240,120}));flat_surface(frame,bounds,*options.color);
    good(host.clear_content());require(!material->set_options(options),"Retired flat material remained writable");material.reset();state->close();
}
void flat_material_sources(){
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task work){work();});
    auto host=take(state->content_host({},"host",{200,100}));std::optional<lvui::native::NativeMaterial> material;ii::gui::Widget source;
    lvui::MaterialConfiguration options;options.kind=lvui::MaterialKind::window;
    options.color=lvui::StyleColor{.25,.5,.75,1};options.tint_opacity=1;options.blur_radius=0;options.radius=0;
    options.inner_highlight_opacity=0;options.border_width=0;options.intense_opacity=0;options.faint_opacity=0;
    options.shadow_opacity=0;options.width=100;options.height=60;options.motion_enabled=false;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
        auto group=builder.zstack({},"group");if(!group)return std::unexpected(group.error());
        auto item=builder.label(*group,"source",{.text="Source"});if(!item)return std::unexpected(item.error());source=*item;
        auto created=lvui::native::NativeMaterial::create(builder,*group,"material",options);
        if(!created)return std::unexpected(created.error());material=*created;return {};
    }));
    auto frame=take(state->drawing_snapshot({200,100}));const auto bounds=take(material->root().snapshot()).bounds;
    flat_surface(frame,bounds,*options.color);
    good(material->set_sources(source,{}));frame=take(state->drawing_snapshot({200,100}));
    require(surface(frame,bounds).backdrop->background_scene&&frame.scenes.size()==1,
        "Opaque material bypassed its explicit source scene");
    good(material->set_sources({},{}));frame=take(state->drawing_snapshot({200,100}));flat_surface(frame,bounds,*options.color);
    state->close();
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
        near(layer.backdrop->tint.alpha,.25)&&near(layer.backdrop->blur_radius,16)&&!layer.backdrop->underlay_tint,
        "Panel composition lost authored style or backdrop");
    require(near(layer.backdrop->underlay_bounds.x,bounds.x-16)&&near(layer.backdrop->underlay_bounds.y,bounds.y-16)&&
        near(layer.backdrop->underlay_bounds.width,bounds.width+32)&&near(layer.backdrop->underlay_bounds.height,bounds.height+32),
        "Material did not provide the blur's outside gradient input");
    require(layer.backdrop->background_scene&&!layer.backdrop->content_scene&&frame.scenes.size()==2,
        "Forward-declared background source was not bound");
    const auto fixed=surface(frame,take(plain.snapshot()).bounds).backdrop;
    require(fixed&&fixed->background_scene&&fixed->content_scene&&*fixed->content_scene==*layer.backdrop->background_scene,
        "Static background and content sources did not share their referenced scenes");
    scope->backend_0.source.set(window);frame=take(state->drawing_snapshot({640,480}));layer=surface(frame,bounds);
    require(layer.backdrop->background_scene&&layer.backdrop->content_scene&&frame.scenes.size()==2,
        "Reactive content source did not enter the material graph");
    auto source_graph=scope->native_lifetime.lock();require(bool(source_graph),"Source binding graph missing");
    scope->backend_0.source.set(panel);frame=take(state->drawing_snapshot({640,480}));layer=surface(frame,bounds);
    require(!take(source_graph->failures()).empty()&&layer.backdrop->content_scene&&frame.scenes.size()==2,
        "Cyclic reactive source replaced the previous valid source");
    scope->backend_0.source.set(window);require(take(source_graph->failures()).empty(),"Valid source did not recover the binding");
    const auto cached=layer.backdrop->underlay;
    frame=take(state->drawing_snapshot({640,480}));require(surface(frame,bounds).backdrop->underlay==cached,"Unchanged material lost its retained gradient");
    require(!surface(frame,take(plain.snapshot()).bounds).backdrop->underlay,"Zero-strength radial texture was generated");
    require(near(surface(frame,take(window.snapshot()).bounds).backdrop->blur_radius,64),"Window density default changed");
    require(take(child.snapshot()).bounds.width>0,"Material did not retain its child");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Material binding graph missing");
    scope->backend_0.density.set(lvui::MaterialDensity::dense75);scope->backend_0.accent.set({0,1,0,1});
    frame=take(state->drawing_snapshot({640,480}));layer=surface(frame,bounds);
    const auto green_image=layer.backdrop->underlay;const auto green_extent=layer.backdrop->underlay_bounds;
    const auto green=radial_pixel(layer,bounds.x+bounds.width/2,bounds.y+bounds.height/2);
    require(near(layer.backdrop->tint.alpha,.75)&&near(layer.backdrop->border.alpha,.12)&&
        green_image&&green_image!=cached&&!layer.backdrop->underlay_tint&&green[1]>green[0],
        "Reactive density or accent did not update the material");
    scope->backend_0.accent.set({0,1,0,.5});frame=take(state->drawing_snapshot({640,480}));layer=surface(frame,bounds);
    const auto center_alpha=[&](const auto& image,const auto& extent){
        const auto x=std::min(image->width()-1,unsigned((bounds.x+bounds.width/2-extent.x)*image->width()/extent.width));
        const auto y=std::min(image->height()-1,unsigned((bounds.y+bounds.height/2-extent.y)*image->height()/extent.height));
        return image->rgba()[(std::size_t(y)*image->width()+x)*4+3];
    };
    require(layer.backdrop->underlay&&layer.backdrop->underlay!=green_image&&!layer.backdrop->underlay_tint&&
        center_alpha(layer.backdrop->underlay,layer.backdrop->underlay_bounds)<center_alpha(green_image,green_extent),
        "Overlapping gradients lost accent alpha");
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
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();source_graph.reset();
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
    auto widget=material->root();auto frame=take(state->drawing_snapshot({200,100}));
    const auto bounds=take(widget.snapshot()).bounds;
    const auto first=radial_pixel(surface(frame,bounds),bounds.x+48,bounds.y+32)[0];
    options.primary_color=lvui::StyleColor{1,0,0,1};good(material->set_options(options));
    const auto anchor=ii::runtime::Clock::now();(void)frames.step(anchor+100ms);
    frame=take(state->drawing_snapshot({200,100}));auto layer=surface(frame,bounds);
    const auto midpoint=radial_pixel(layer,bounds.x+48,bounds.y+32)[0];
    (void)frames.step(anchor+250ms);frame=take(state->drawing_snapshot({200,100}));
    const auto last=radial_pixel(surface(frame,bounds),bounds.x+48,bounds.y+32)[0];
    require(first<midpoint&&midpoint<last,"Material color transition did not reach its midpoint and finish");
    bool rejected=false;std::thread worker([&]{auto changed=material->set_options(options);rejected=!changed&&changed.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread changed material");good(host.clear_content());
    auto closed=material->set_options(options);require(!closed&&closed.error().code==ii::core::ErrorCode::closed,"Retired material remained writable");
    material.reset();state->close();
}
}
int main(){
    lvui::native::MaterialModel material;require(material.set_configuration({},{}),"Default material was rejected");
    lvui::native::NativeMaterialGradient cache;
    auto normal=take(cache.render(material,100,50,1));
    require(normal&&normal->width()==100&&normal->height()==50&&cache.render(material,100,50,1).value()==normal,
            "Material gradient did not retain its 1x raster");
    auto high=take(cache.render(material,100,50,2));
    require(high&&high!=normal&&high->width()==200&&high->height()==100,
            "Material gradient did not follow backing scale");
    auto padded=take(cache.render(material,100,50,1,16,8));
    require(padded&&padded->width()==132&&padded->height()==66&&
            cache.render(material,100,50,1,16,8).value()==padded&&
            padded->rgba()[(std::size_t(33)*padded->width())*4+3]>0,
            "Material gradient omitted or failed to cache outside blur pixels");
    auto padded_high=take(cache.render(material,100,50,2,16,8));
    require(padded_high&&padded_high->width()==264&&padded_high->height()==132,
            "Padded material gradient did not follow backing scale");
    require(!cache.render(material,100,50,1,-1,0)&&!cache.render(material,8192,50,1,1,0),
            "Material gradient accepted invalid padding");
    const auto limited=lvui::native::NativeMaterialGradient::blur_padding(8191,8190,16);
    require(near(limited.first,.5)&&near(limited.second,1)&&
            lvui::native::NativeMaterialGradient::blur_padding(100,50,0)==std::pair{0.,0.},
            "Material blur padding exceeded the host limit or appeared without blur");
    require(!cache.render(material,100,50,0),"Material gradient accepted an invalid backing scale");
    flat_material();flat_material_sources();generated();lifecycle_and_motion();
}
