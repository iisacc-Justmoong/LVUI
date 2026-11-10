#include "native_popover_composition.h"
#include "NativeOwnedPopover.generated.h"
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& tree,std::string_view suffix){
    const auto nodes=take(tree->snapshots());const auto item=std::ranges::find_if(nodes,[&](const auto& value){return value.id.ends_with(suffix);});
    if(item==nodes.end())throw std::runtime_error("Generated Popover node did not mount");return take(tree->find(item->id));
}
void centered_popover(){
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"center-host",{500,300}));
    std::optional<lvui::native::NativePopover> popup;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        lvui::PopoverConfiguration options;options.width=200;options.height=100;
        options.placement=lvui::PopoverPlacement::centered;
        auto value=lvui::native::NativePopover::create(builder,{},"centered",options);
        if(!value)return std::unexpected(value.error());popup=*value;return {};
    }));
    good(popup->root().open_at(10,10));
    const auto frame=take(tree->drawing_snapshot({500,300}));
    const auto bounds=take(popup->root().snapshot()).bounds;
    require(bounds.x==150 && bounds.y==100 && bounds.width==200 && bounds.height==100,
            "Centered popover ignored its host placement");
    const auto surface=std::ranges::find_if(frame.overlays,[&](const auto& layer){
        return layer.backdrop&&layer.backdrop->bounds==bounds&&layer.shadow;
    });
    const auto source=lvui::figma_defaults::panel_surface_solid;
    const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    require(surface!=frame.overlays.end()&&surface->backdrop->blur_radius==16&&surface->backdrop->border_width==1&&
            std::abs(surface->backdrop->inner_highlight_opacity-.14)<.0001&&
            std::abs(surface->backdrop->inner_highlight_offset_y-1)<.0001&&
            std::abs(surface->backdrop->border.alpha-.22)<.0001&&
            std::abs(surface->backdrop->tint.red-linear(source.red))<.0001&&
            std::abs(surface->backdrop->tint.alpha-.25)<.0001&&
            surface->shadow->blur_radius==24&&surface->shadow->bounds.y==bounds.y+8&&
            std::abs(surface->shadow->color.alpha-.22)<.0001,
            "Popover did not use the shared LVUI glass material and elevation");
    const auto underlay=surface->backdrop->underlay;
    const ii::gui::Rect padded_bounds{bounds.x-16,bounds.y-16,bounds.width+32,bounds.height+32};
    require(underlay&&surface->backdrop->underlay_bounds==padded_bounds&&!surface->backdrop->underlay_tint&&
            underlay->width()==232&&underlay->height()==132,
            "Popover did not rasterize its common radial accents at the actual extent");
    bool colored=false;const auto pixels=underlay->rgba();
    for(std::size_t i=3;i<pixels.size();i+=4)colored|=pixels[i]>0;
    require(colored,"Popover radial accent image was empty");
    const auto repeat=take(tree->drawing_snapshot({500,300}));
    require(std::ranges::any_of(repeat.overlays,[&](const auto& layer){return layer.backdrop&&layer.backdrop->underlay==underlay;}),
            "Popover did not reuse its radial accent raster");
    auto options=take(popup->options());options.width=240;good(popup->set_options(options));
    const auto resized=take(tree->drawing_snapshot({500,300}));
    require(std::ranges::any_of(resized.overlays,[&](const auto& layer){return layer.backdrop&&layer.backdrop->underlay&&
        layer.backdrop->underlay!=underlay&&layer.backdrop->underlay->width()==272&&layer.backdrop->underlay->height()==132;}),
        "Popover radial accent did not follow resizing");
    const auto retained=std::ranges::find_if(resized.overlays,[&](const auto& layer){return layer.backdrop&&layer.backdrop->underlay;});
    const auto resized_mask=retained->backdrop->underlay;
    options.primary_color=lvui::StyleColor{.8,.2,.1,1};good(popup->set_options(options));
    const auto recolored=take(tree->drawing_snapshot({500,300}));
    const auto changed=std::ranges::find_if(recolored.overlays,[&](const auto& layer){return layer.backdrop&&layer.backdrop->underlay;});
    require(changed!=recolored.overlays.end()&&changed->backdrop->underlay!=resized_mask&&!changed->backdrop->underlay_tint&&
            !std::ranges::equal(changed->backdrop->underlay->rgba(),resized_mask->rgba()),
            "Changing Popover primary color did not recolor its radial image");
    const auto recolored_image=changed->backdrop->underlay;
    auto invalid=options;invalid.primary_color=lvui::StyleColor{2,0,0,1};
    require(!popup->set_options(invalid)&&take(popup->options()).primary_color==options.primary_color,
            "Invalid Popover primary color replaced its last valid configuration");
    const auto after_invalid=take(tree->drawing_snapshot({500,300}));
    require(std::ranges::any_of(after_invalid.overlays,[&](const auto& layer){return layer.backdrop&&
        layer.backdrop->underlay==recolored_image&&!layer.backdrop->underlay_tint;}),
        "Invalid Popover primary color changed its rendered material");
    options.primary_color=lvui::StyleColor{.8,.2,.1,.5};good(popup->set_options(options));
    const auto translucent=take(tree->drawing_snapshot({500,300}));
    require(std::ranges::any_of(translucent.overlays,[&](const auto& layer){return layer.backdrop&&
        layer.backdrop->underlay&&layer.backdrop->underlay!=recolored_image;}),
        "Popover did not regenerate its radial image after primary alpha changed");
    good(host.clear_content());tree->close();
}
void generated_motion(){
    using namespace std::chrono_literals;
    using View=lvui_generated::NativeOwnedPopover;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames{events};
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},
        ii::gui::WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"motion-host",{500,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto popup=find(tree,"/popup");(void)take(tree->drawing_snapshot({500,300}));
    auto start=ii::runtime::Clock::now();good(popup.open_at(40,40));(void)frames.step(start+1h);
    good(popup.close_popup());(void)frames.step(start+2h);good(popup.open_at(40,40));start+=2h;
    (void)frames.step(start+80ms);require(take(popup.snapshot()).transitioning,
        "Generated Popover did not start its motion");
    const auto entering=take(tree->drawing_snapshot({500,300}));
    require(std::ranges::any_of(entering.overlays,[](const auto& layer){return layer.backdrop&&layer.shadow&&
        layer.backdrop->underlay&&layer.backdrop->border_width==1&&
        std::abs(layer.backdrop->tint.alpha-.25)<.0001&&layer.content_transform.scale<1&&
        std::abs(layer.content_transform.opacity-.5)<.01;}),
        "Generated Popover motion did not transform the complete glass material");
    scope->backend_0.motion.set(false);
    require(take(popup.snapshot()).opened&&!take(popup.snapshot()).transitioning,
        "Reactive motionEnabled did not settle the generated Popover");
    scope->backend_0.motion.set(true);good(popup.close_popup());(void)frames.step(start+155ms);
    require(take(popup.snapshot()).visible&&take(popup.snapshot()).exiting,
        "Re-enabled generated Popover did not retain its exit frame");
    const auto exiting=take(tree->drawing_snapshot({500,300}));
    require(std::ranges::any_of(exiting.overlays,[](const auto& layer){return layer.backdrop&&layer.shadow&&
        layer.backdrop->underlay&&layer.content_transform.opacity>0&&layer.content_transform.opacity<1;}),
        "Generated Popover lost its glass material during exit");
    scope->backend_0.motion.set(false);
    require(!take(popup.snapshot()).visible,"Reactive motionEnabled did not settle the exit");
    auto graph=scope->native_lifetime.lock();require(graph&&take(graph->failures()).empty(),
        "Reactive Popover motion produced a binding error");
    good(host.clear_content());tree->close();
}
}
int main(){
    centered_popover();
    generated_motion();
    using View=lvui_generated::NativeOwnedPopover;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{},[](auto&){},&scope);}));
    auto popup=find(tree,"/popup"),label=find(tree,"/label");good(popup.open_at(40,40));auto first=take(popup.snapshot());
    require(first.opened&&first.bounds.width==180&&first.bounds.height==80&&!first.modal&&first.dim&&
        first.accessibility_name=="Details"&&scope->backend_0.opens==1,"Independent Popover did not open with its configured surface");
    auto has_blur=[&]{const auto f=take(tree->drawing_snapshot({500,300}));const auto bounds=take(popup.snapshot()).bounds;
        return std::ranges::any_of(f.overlays,[&](const auto& layer){return layer.backdrop&&
            layer.backdrop->bounds==bounds&&layer.backdrop->blur_radius==16;});};
    require(has_blur(),"Generated Popover did not use the glass material");
    const auto custom_tint=[&]{const auto f=take(tree->drawing_snapshot({500,300}));const auto bounds=take(popup.snapshot()).bounds;
        const auto layer=std::ranges::find_if(f.overlays,[&](const auto& value){return value.backdrop&&value.backdrop->bounds==bounds;});
        if(layer==f.overlays.end())throw std::runtime_error("Popover tint is unavailable");return layer->backdrop->tint;};
    const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    require(std::abs(custom_tint().red-linear(.2))<.0001&&std::abs(custom_tint().alpha-.25)<.0001,
            "Popover ignored its authored tint and opacity");
    scope->backend_0.color.set({.4,.2,.2,1});
    require(std::abs(custom_tint().red-linear(.4))<.0001,"Reactive Popover tint did not update");
    scope->backend_0.blur.set(false);require(!has_blur(),"Disabling Popover blur did not select its opaque fallback");
    const auto fallback=take(tree->drawing_snapshot({500,300}));const auto popup_bounds=take(popup.snapshot()).bounds;
    require(std::ranges::any_of(fallback.overlays,[&](const auto& layer){return layer.backdrop&&
        layer.backdrop->bounds==popup_bounds&&!layer.backdrop->capture&&layer.backdrop->base.alpha==1&&
        layer.backdrop->blur_radius==0&&layer.backdrop->underlay&&!layer.backdrop->underlay_tint&&
        layer.backdrop->underlay_bounds==popup_bounds&&layer.backdrop->border_width==1&&
        std::abs(layer.backdrop->border.alpha-.22)<.0001&&
        std::abs(layer.backdrop->inner_highlight_opacity-.14)<.0001;
    }),"Popover fallback lost its isolated opaque LVUI material");
    scope->backend_0.blur.set(true);require(has_blur(),"Popover did not recover its glass material");
    scope->backend_0.width.set(220);scope->backend_0.modal.set(true);scope->backend_0.text.set("Updated");
    auto changed=take(popup.snapshot());require(changed.opened&&changed.bounds.width==220&&changed.modal&&take(label.snapshot()).text=="Updated",
        "Reactive Popover update replaced or lost its content");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Generated Popover graph is unavailable");
    scope->backend_0.width.set(-1);require(!take(graph->failures()).empty()&&take(popup.snapshot()).bounds.width==220,"Invalid Popover geometry changed its surface");
    scope->backend_0.width.set(180);require(take(graph->failures()).empty()&&take(popup.snapshot()).bounds.width==180,"Popover did not recover after invalid geometry");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    require(!take(popup.snapshot()).opened&&scope->backend_0.closes==1&&scope->backend_0.cancels==1,"Popover cancellation callbacks failed");
    good(popup.open_at(40,40));scope->backend_0.onClose=[&]{scope->backend_0.onClose={};good(popup.open_at(50,50));};
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    require(take(popup.snapshot()).opened&&scope->backend_0.opens==3&&scope->backend_0.closes==2&&scope->backend_0.cancels==1,
        "An old cancellation reached a reopened Popover");good(popup.close_popup());
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!popup.snapshot(),"Popover retained its activation");tree->close();
}
