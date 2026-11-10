#include "NativeOwnedSlider.generated.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<.001;}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void generated(){
    using View=lvui_generated::NativeOwnedSlider;using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{640,420}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](auto& builder){return View::build(builder,{}, {},&scope);}));
    auto nodes=take(tree->snapshots());auto found=std::ranges::find_if(nodes,[](const auto& node){return node.id.ends_with("/slider");});
    require(found!=nodes.end(),"Generated Slider is missing");auto slider=take(tree->find(found->id));auto initial=take(slider.snapshot());
    require(initial.kind==WidgetKind::action_surface&&initial.value==5&&initial.accessibility_semantics&&
        initial.accessibility_semantics->role==AccessibilityRole::slider&&initial.size.height==22,"Generated Slider is not LVUI-owned or did not apply its size");
    good(slider.focus());
    // The headless root is not attached to a platform surface. Flush the
    // published child generation before inspecting its animated focus state.
    (void)take(slider.snapshot());
    const auto focused=take(tree->drawing_snapshot({640,420}));
    const auto green_ring=[](const auto& panels){return std::ranges::any_of(panels,[](const auto& panel){
        return panel.stroke_width==2&&panel.top.red==0&&panel.top.green==1&&panel.top.blue==0&&panel.top.alpha==1;
    });};
    require(green_ring(focused.panels)||std::ranges::any_of(focused.overlays,[&](const auto& layer){return green_ring(layer.panels);}),
        "Published generated Slider did not use its custom focus color");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=initial.bounds.x+92,.y=initial.bounds.y+11}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=initial.bounds.x+92,.y=initial.bounds.y+11}));
    require(take(slider.snapshot()).value==10&&scope->backend_0.value.get()==10&&scope->backend_0.values.size()==1,
        "Generated Slider did not deliver its LVUI movement callback");
    good(tree->set_accessibility_value(initial.id,6.4));
    require(take(slider.snapshot()).value==6&&scope->backend_0.value.get()==6&&scope->backend_0.values.back()==6&&scope->backend_0.values.size()==2,
        "Generated Slider did not connect accessible value setting to its view model");
    scope->backend_0.value.set(4);scope->backend_0.width.set(120);
    auto changed=take(slider.snapshot());require(changed.value==4&&changed.size.width==120,"Generated Slider did not apply reactive values");
    auto graph=scope->native_lifetime.lock();scope->backend_0.size.set(lvui::SliderSize::large);
    require(take(slider.snapshot()).size.height==44&&take(slider.snapshot()).value==4,"Reactive Slider size did not preserve its value");
    scope->backend_0.size.set(static_cast<lvui::SliderSize>(99));
    require(!take(graph->failures()).empty()&&take(slider.snapshot()).size.height==44,"Invalid reactive size replaced the Slider state");
    scope->backend_0.size.set(lvui::SliderSize::mini);require(take(graph->failures()).empty(),"Slider did not recover after an invalid size");
    const auto centers=[&](){const auto frame=take(tree->drawing_snapshot({640,420}));std::size_t count=0;
        const auto scan=[&](const auto& panels){for(const auto& p:panels)if(p.bounds.width==2&&p.bounds.height==8&&p.top.alpha==.5f)++count;};
        scan(frame.panels);for(const auto& layer:frame.overlays)scan(layer.panels);return count;};
    require(centers()==0,"Normal generated Slider showed a center mark");
    scope->backend_0.type.set(lvui::SliderType::center_biased);
    require(centers()==1&&take(slider.snapshot()).value==4,"Reactive Slider type lost the center mark or value");
    scope->backend_0.type.set(lvui::SliderType::center_biased_ticks);
    const auto frame_without_ticks=take(tree->drawing_snapshot({640,420}));std::size_t dots=0;
    const auto count_dots=[&](const auto& panels){for(const auto& p:panels)if(p.bounds.width==2&&p.bounds.height==2)++dots;};
    count_dots(frame_without_ticks.panels);for(const auto& layer:frame_without_ticks.overlays)count_dots(layer.panels);
    require(centers()==1&&dots==0,"Explicit tick settings did not override the reactive Slider preset");
    scope->backend_0.type.set(static_cast<lvui::SliderType>(99));
    require(!take(graph->failures()).empty()&&centers()==1,"Invalid reactive Slider type replaced the last valid view");
    scope->backend_0.type.set(lvui::SliderType::normal);
    require(take(graph->failures()).empty()&&centers()==0,"Generated Slider did not recover its normal variant");
    scope->backend_0.maximum.set(-1);
    require(!take(graph->failures()).empty()&&take(slider.snapshot()).value==4,"Invalid reactive range replaced the Slider state");
    scope->backend_0.maximum.set(10);require(take(graph->failures()).empty(),"Generated Slider did not recover after an invalid range");
    const auto ticks=[&](){const auto frame=take(tree->drawing_snapshot({640,420}));std::size_t count=0;
        const auto scan=[&](const auto& panels){for(const auto& p:panels)if(p.bounds.width==2&&p.bounds.height==2)++count;};
        scan(frame.panels);for(const auto& layer:frame.overlays)scan(layer.panels);return count;};
    require(ticks()==0,"Generated Slider showed ticks before enabling them");
    scope->backend_0.showTicks.set(true);require(ticks()==11,"Generated Slider did not show the default number of ticks");
    scope->backend_0.tickCount.set(3);require(ticks()==3,"Generated Slider did not apply the reactive tick count");
    scope->backend_0.tickCount.set(3.5);require(!take(graph->failures()).empty()&&ticks()==3,"Invalid reactive tick count replaced the Slider state");
    scope->backend_0.tickCount.set(2);require(take(graph->failures()).empty()&&ticks()==2,"Slider did not recover after an invalid tick count");
    scope->backend_0.showTicks.set(false);require(ticks()==0,"Generated Slider did not hide its ticks");
    const auto images=[&](){const auto frame=take(tree->drawing_snapshot({640,420}));std::size_t count=frame.images.size();
        for(const auto& layer:frame.overlays)count+=layer.images.size();return count;};
    const auto symbol_image=[&]()->std::shared_ptr<const RasterImage>{const auto frame=take(tree->drawing_snapshot({640,420}));
        if(!frame.images.empty())return frame.images.front().image;
        for(const auto& layer:frame.overlays)if(!layer.images.empty())return layer.images.front().image;return {};};
    require(images()==0,"Generated Slider showed a symbol before enabling it");
    scope->backend_0.showSymbol.set(true);require(images()==1,"Generated Slider did not show its Figma symbol");
    const auto original_symbol=symbol_image();scope->backend_0.symbolName.set("generaladd");const auto alternate_symbol=symbol_image();
    require(original_symbol&&alternate_symbol&&take(graph->failures()).empty()&&
            !std::ranges::equal(original_symbol->rgba(),alternate_symbol->rgba()),"Generated Slider did not replace its symbol");
    scope->backend_0.symbolName.set("missing-slider-symbol");
    require(!take(graph->failures()).empty()&&symbol_image()==alternate_symbol,"A missing symbol replaced the last valid Slider display");
    scope->backend_0.symbolName.set("sliderSymbolSun");
    require(take(graph->failures()).empty()&&images()==1,"Slider did not recover its symbol after an invalid update");
    scope->backend_0.showSymbol.set(false);require(images()==0,"Generated Slider did not hide its symbol");
    const auto label_bounds=[&](std::string_view text)->std::optional<Rect>{const auto frame=take(tree->drawing_snapshot({640,420}));
        for(const auto& run:frame.texts)if(run.text==text)return run.bounds;
        for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return run.bounds;
        return std::nullopt;};
    scope->backend_0.showMinMax.set(true);auto low=label_bounds("0%"),high=label_bounds("100%");
    require(low&&high&&low->x<high->x,"Generated Slider did not display endpoint labels");
    const auto label_color=[&](std::string_view text)->std::optional<Color>{const auto frame=take(tree->drawing_snapshot({640,420}));
        for(const auto& run:frame.texts)if(run.text==text)return run.color;
        for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return run.color;
        return std::nullopt;};
    scope->backend_0.labelColor.set(lvui::StyleColor{.25,.5,1,.75});
    auto color=label_color("0%"),other_color=label_color("100%");
    require(color&&other_color&&*color==*other_color&&near(color->red,.0508761)&&near(color->green,.21404114)&&
        near(color->blue,1)&&near(color->alpha,.75),"Reactive Slider label color did not update both endpoints");
    scope->backend_0.labelColor.set(lvui::StyleColor{-1,.5,1,.75});
    require(!take(graph->failures()).empty()&&label_color("0%")==color,
        "Invalid reactive Slider label color replaced the last valid drawing");
    scope->backend_0.labelColor.set(lvui::StyleColor{1,0,0,0});color=label_color("0%");
    require(take(graph->failures()).empty()&&color&&near(color->red,1)&&near(color->alpha,0),
        "Slider did not recover a transparent label color after an invalid update");
    scope->backend_0.minimumLabel.set("Low");require(label_bounds("Low").has_value(),"Reactive endpoint label did not update");
    scope->backend_0.rightToLeft.set(true);low=label_bounds("Low");high=label_bounds("100%");
    require(low&&high&&low->x>high->x,"Generated Slider did not mirror its endpoint labels");
    scope->backend_0.showLabels.set(false);scope->backend_0.showEndpointIcons.set(true);
    require(!label_bounds("Low")&&images()==2,"Generated Slider did not switch from labels to endpoint icons");
    scope->backend_0.minimumIconName.set("missing-slider-endpoint");
    require(!take(graph->failures()).empty()&&images()==2,"Invalid endpoint icon replaced the last valid Slider display");
    scope->backend_0.minimumIconName.set("sliderSymbolSun");require(take(graph->failures()).empty(),"Slider did not recover its endpoint icon");
    scope->backend_0.showMinMax.set(false);require(images()==0,"Hidden Slider endpoints retained their icons");
    scope->backend_0.size.set(lvui::SliderSize::regular);scope->backend_0.type.set(lvui::SliderType::filled);
    scope->backend_0.inactiveFillColor.set(lvui::StyleColor{1,0,0,.5});scope->backend_0.active.set(false);
    const auto filled_panels=[&](){const auto frame=take(tree->drawing_snapshot({640,420}));std::vector<Panel> result;
        const auto collect=[&](const auto& panels){for(const auto& panel:panels)
            if(near(panel.bounds.height,28)&&near(panel.stroke_width,0))result.push_back(panel);};
        collect(frame.panels);for(const auto& layer:frame.overlays)collect(layer.panels);return result;};
    auto inactive=filled_panels();
    require(inactive.size()==1&&near(inactive[0].top.red,1)&&near(inactive[0].top.green,0)&&near(inactive[0].top.alpha,.5),
        "Reactive inactive Slider did not display its selected inactive fill");
    good(tree->input({.kind=WidgetInputKind::increment,.target=initial.id}));
    require(take(slider.snapshot()).value==5,"Inactive fill incorrectly disabled Slider input");
    scope->backend_0.value.set(4);
    scope->backend_0.active.set(true);auto active=filled_panels();
    require(active.size()==2&&active[0].bounds==active[1].bounds&&near(active[0].top.alpha,.5)&&
                near(active[1].top.alpha,1)&&take(slider.snapshot()).value==4,
        "Reactive Slider activation lost its fill order or current value");
    scope->backend_0.inactiveFillColor.set(lvui::StyleColor{-1,0,0,.5});
    require(!take(graph->failures()).empty()&&filled_panels()==active,
        "Invalid reactive inactive fill replaced the last valid drawing");
    scope->backend_0.inactiveFillColor.set(lvui::StyleColor{1,0,0,.5});
    require(take(graph->failures()).empty(),"Slider did not recover its inactive fill color");
    scope->backend_0.type.set(lvui::SliderType::segmented);scope->backend_0.showTicks.set(true);
    const auto segment_ticks=[&](){const auto frame=take(tree->drawing_snapshot({640,420}));std::size_t count=0;
        const auto scan=[&](const auto& panels){for(const auto& p:panels)if(near(p.bounds.width,4)&&near(p.bounds.height,4)&&near(p.top.alpha,.5))++count;};
        scan(frame.panels);for(const auto& layer:frame.overlays)scan(layer.panels);return count;};
    require(segment_ticks()==5,"Reactive segmented Slider did not show five marks");
    scope->backend_0.segmentCount.set(3);require(segment_ticks()==3,"Reactive segment count did not change the marks");
    scope->backend_0.segmentCount.set(3.5);
    require(!take(graph->failures()).empty()&&segment_ticks()==3,"Invalid segment count replaced the last valid view");
    scope->backend_0.segmentCount.set(5);require(take(graph->failures()).empty()&&segment_ticks()==5,"Segment count did not recover");
    good(tree->input({.kind=WidgetInputKind::increment,.target=initial.id}));
    require(take(slider.snapshot()).value==5,"Explicit step did not override the reactive segmented default");
    scope->backend_0.value.set(4);scope->backend_0.showTicks.set(false);
    scope->backend_0.type.set(lvui::SliderType::min_max_labels);
    require(segment_ticks()==0&&take(slider.snapshot()).value==4,"Reactive labeled variant lost its explicit settings or value");
    scope->backend_0.enabled.set(false);good(tree->input({.kind=WidgetInputKind::increment,.target=initial.id}));
    require(take(slider.snapshot()).value==4,"Disabled generated Slider accepted accessibility input");
    good(host.clear_content());scope.reset();graph.reset();require(!slider.snapshot(),"Retired generated Slider remained active");tree->close();
}
}
int main(){generated();}
