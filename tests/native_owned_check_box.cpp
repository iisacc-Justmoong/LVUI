#include "NativeOwnedCheckBox.generated.h"
#include <LVUI/IconCatalog.h>
#include <LVUI/NativeAuthoredIcons.h>
#include <iiGUI/SvgImageCache.h>
#include <iiGUI/NativeCheckBoxView.h>
using CheckBoxView=lvui::native::CheckBoxBinding<ii::gui::BasicCheckBox,ii::gui::BasicNativeCheckBox,ii::gui::CachedSvgDecoder>;
#include <iiGUI/Image.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
bool near(double a,double b){return std::abs(a-b)<1e-4;}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state,std::string_view suffix){
    const auto snapshots=take(state->snapshots());
    const auto found=std::ranges::find_if(snapshots,[&](const auto& item){return item.id.ends_with(suffix);});
    if(found==snapshots.end())throw std::runtime_error("Checkbox did not mount");return take(state->find(found->id));
}
struct Ink final {ii::gui::Color fill;float radius=0;std::shared_ptr<const ii::gui::RasterImage> mark;std::optional<ii::gui::Color> tint;};
Ink ink(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    Ink result;bool found=false;
    const auto scan=[&](const auto& panels,const auto& images){
        for(const auto& panel:panels)if(near(panel.bounds.x,bounds.x+.5)&&near(panel.bounds.y,bounds.y+(bounds.height-17)/2)&&
            near(panel.bounds.width,17)&&near(panel.bounds.height,17)&&near(panel.stroke_width,0)){
            result.fill=panel.top;result.radius=panel.radius;found=true;
        }
        for(const auto& image:images)if(!result.mark&&near(image.bounds.x,bounds.x+.5)&&near(image.bounds.y,bounds.y+(bounds.height-17)/2)&&
            near(image.bounds.width,17)&&near(image.bounds.height,17)){result.mark=image.image;result.tint=image.tint;}
    };
    scan(frame.panels,frame.images);for(const auto& layer:frame.overlays)scan(layer.panels,layer.images);
    if(!found)throw std::runtime_error("Checkbox indicator was not painted");return result;
}
bool has_inner_shadow(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    const auto scan=[&](const auto& panels){return std::ranges::any_of(panels,[&](const auto& panel){
        return near(panel.bounds.x,bounds.x+.5)&&near(panel.bounds.y,bounds.y+(bounds.height-17)/2)&&near(panel.bounds.width,17)&&
            near(panel.bounds.height,17)&&near(panel.stroke_width,1)&&panel.top.alpha>0;
    });};
    if(scan(frame.panels))return true;for(const auto& layer:frame.overlays)if(scan(layer.panels))return true;return false;
}
std::optional<ii::gui::Color> inner_shadow(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    std::optional<ii::gui::Color> result;
    const auto scan=[&](const auto& panels){for(const auto& panel:panels)
        if(near(panel.bounds.x,bounds.x+.5)&&near(panel.bounds.y,bounds.y+(bounds.height-17)/2)&&near(panel.bounds.width,17)&&
           near(panel.bounds.height,17)&&near(panel.stroke_width,1))result=panel.top;};
    scan(frame.panels);for(const auto& layer:frame.overlays)scan(layer.panels);return result;
}
std::optional<ii::gui::Color> upper_shadow(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    std::optional<ii::gui::Color> result;
    const auto scan=[&](const auto& panels){for(const auto& panel:panels)
        if(near(panel.bounds.x,bounds.x+.5)&&near(panel.bounds.y,bounds.y+(bounds.height-17)/2)&&near(panel.bounds.width,17)&&
           near(panel.bounds.height,17*.45)&&near(panel.stroke_width,0))result=panel.top;};
    scan(frame.panels);for(const auto& layer:frame.overlays)scan(layer.panels);return result;
}
std::optional<ii::gui::ImageRun> centered_mark(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds,double padding=.5){
    std::optional<ii::gui::ImageRun> result;
    const auto scan=[&](const auto& images){for(const auto& image:images)
        if(near(image.bounds.x+image.bounds.width/2,bounds.x+padding+8.5)&&
           near(image.bounds.y+image.bounds.height/2,bounds.y+bounds.height/2))result=image;};
    scan(frame.images);for(const auto& layer:frame.overlays)scan(layer.images);return result;
}
void interaction(){
    using View=lvui_generated::NativeOwnedCheckBox;
    require(lvui::native::checkbox_shape(0)==lvui::CheckBoxShape::round_rect&&
            lvui::native::checkbox_shape(1.)==lvui::CheckBoxShape::cylinder&&
            lvui::native::checkbox_shape(.5)!=lvui::CheckBoxShape::cylinder&&
            lvui::native::checkbox_shape(.5)!=lvui::CheckBoxShape::round_rect,
            "Native numeric checkbox shape conversion failed");
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{640,480}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));
    auto check=find(state,"/check");auto initial=take(check.snapshot());
    require(initial.kind==ii::gui::WidgetKind::action_surface&&initial.action_role==ii::gui::ActionRole::check_box&&
            initial.accessibility_name=="Remember choice"&&!initial.checked&&near(initial.bounds.height,18),
            "Checkbox host semantics or initial state is wrong");
    auto initial_drawing=take(state->drawing_snapshot({640,480}));
    require(!ink(initial_drawing,initial.bounds).mark&&has_inner_shadow(initial_drawing,initial.bounds),
            "Unchecked checkbox did not draw its inner shadow or painted a mark");
    auto stock=find(state,"/stock");const auto stock_visual=ink(initial_drawing,take(stock.snapshot()).bounds);
    auto stock_expected=take(lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("checkboxCheckedEnabled",68));
    const auto linear_color=[](double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
    bool stock_ink=false,stock_clear=false;for(std::size_t n=3;n<stock_expected->rgba().size();n+=4){
        stock_ink|=stock_expected->rgba()[n]>0;stock_clear|=stock_expected->rgba()[n]==0;
    }
    require(stock_visual.mark&&std::ranges::equal(stock_visual.mark->rgba(),stock_expected->rgba())&&!stock_visual.tint&&
            stock_ink&&stock_clear&&near(stock_visual.fill.red,linear_color(lvui::figma_defaults::accent_blue_default.red)),
            "A default generated checkbox did not select the original checked asset");
    scope->backend_0.softShadow.set({1,0,0,.4});
    auto shadow=inner_shadow(take(state->drawing_snapshot({640,480})),initial.bounds);
    require(shadow&&near(shadow->red,1)&&near(shadow->alpha,.4),"Reactive inner shadow color did not update");
    auto diagnostics=scope->native_lifetime.lock();require(bool(diagnostics),"Checkbox diagnostics unavailable");
    scope->backend_0.softShadow.set({2,0,0,1});shadow=inner_shadow(take(state->drawing_snapshot({640,480})),initial.bounds);
    require(!take(diagnostics->failures()).empty()&&shadow&&near(shadow->alpha,.4),"Invalid inner shadow replaced the retained color");
    scope->backend_0.softShadow.set({0,0,0,20./255.});require(take(diagnostics->failures()).empty(),"Inner shadow did not recover");
    scope->backend_0.strongShadow.set({0,0,1,.4});
    auto upper=upper_shadow(take(state->drawing_snapshot({640,480})),initial.bounds);
    require(upper&&near(upper->blue,1)&&near(upper->alpha,.2),"Reactive upper shadow did not preserve its color and opacity");
    scope->backend_0.strongShadow.set({0,0,0,26./255.});
    diagnostics.reset();
    const auto pointer=[&](ii::gui::WidgetInputKind kind,double x,double y){good(state->input({.kind=kind,.x=x,.y=y}));};
    const auto x=initial.bounds.x+8,y=initial.bounds.y+9;
    pointer(ii::gui::WidgetInputKind::pointer_down,x,y);
    require(scope->backend_0.events.empty(),"Pointer press committed early");
    pointer(ii::gui::WidgetInputKind::pointer_up,x,y);
    require(take(check.snapshot()).checked&&scope->backend_0.events==std::vector<bool>{true},"Click did not commit once");
    pointer(ii::gui::WidgetInputKind::pointer_move,initial.bounds.x-20,initial.bounds.y-20);
    auto visual=ink(take(state->drawing_snapshot({640,480})),take(check.snapshot()).bounds);
    require(visual.mark&&visual.mark->width()==68&&visual.mark->height()==68&&visual.tint&&near(visual.tint->alpha,.8),
            "Checkbox did not paint its independent mark");
    const auto pixels=visual.mark->rgba();bool clear=false,solid=false;
    for(std::size_t n=3;n<pixels.size();n+=4){clear|=pixels[n]==0;solid|=pixels[n]==255;}
    require(clear&&solid,"Check mark mask has no distinct ink and empty regions");
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Checkbox binding graph unavailable");
    scope->backend_0.color.set({.5,.25,.75,.75});
    visual=ink(take(state->drawing_snapshot({640,480})),take(check.snapshot()).bounds);
    require(near(visual.fill.red,.21404114)&&near(visual.fill.green,.05087609)&&near(visual.fill.blue,.52252155)&&near(visual.fill.alpha,.75)&&
            take(check.snapshot()).checked,"Reactive color or linear-light conversion failed");
    scope->backend_0.useAsset.set(true);
    visual=ink(take(state->drawing_snapshot({640,480})),take(check.snapshot()).bounds);
    auto expected=take(lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("checkboxCheckedEnabled",68));
    const auto linear=[](double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
    require(visual.mark&&std::ranges::equal(visual.mark->rgba(),expected->rgba())&&!visual.tint&&
            near(visual.fill.red,linear(.5)),
            "Reactive Figma checked asset did not draw the authored curve and fill");
    scope->backend_0.shape.set(lvui::CheckBoxShape::cylinder);
    visual=ink(take(state->drawing_snapshot({640,480})),take(check.snapshot()).bounds);
    require(near(visual.radius,8.5)&&visual.mark&&!std::ranges::equal(visual.mark->rgba(),expected->rgba())&&near(visual.fill.red,.21404114),
            "Reactive cylindrical shape did not use its round custom indicator");
    scope->backend_0.shape.set(static_cast<lvui::CheckBoxShape>(42));
    require(!take(graph->failures()).empty()&&near(ink(take(state->drawing_snapshot({640,480})),take(check.snapshot()).bounds).radius,8.5),
            "Invalid checkbox shape replaced the last valid indicator");
    scope->backend_0.shape.set(lvui::CheckBoxShape::round_rect);
    visual=ink(take(state->drawing_snapshot({640,480})),take(check.snapshot()).bounds);
    require(take(graph->failures()).empty()&&near(visual.radius,3.5)&&visual.mark&&
            std::ranges::equal(visual.mark->rgba(),expected->rgba()),"Checkbox shape did not recover its original asset");
    scope->backend_0.useAsset.set(false);
    visual=ink(take(state->drawing_snapshot({640,480})),take(check.snapshot()).bounds);
    require(visual.mark&&!std::ranges::equal(visual.mark->rgba(),expected->rgba())&&near(visual.fill.red,.21404114),
            "Disabling the stock asset did not restore the custom mark and fill");
    scope->backend_0.color.set({2,0,0,1});
    require(!take(graph->failures()).empty()&&ink(take(state->drawing_snapshot({640,480})),take(check.snapshot()).bounds).fill==visual.fill,
            "Invalid color changed the retained checkbox");
    scope->backend_0.color.set({.5,.25,.75,.75});require(take(graph->failures()).empty(),"Checkbox did not recover from invalid color");
    scope->backend_0.duration.set(1.5);require(!take(graph->failures()).empty()&&take(check.snapshot()).checked,
        "Invalid reactive motion duration changed the retained checkbox");
    scope->backend_0.duration.set(360);require(take(graph->failures()).empty(),"Checkbox motion duration did not recover");
    scope->backend_0.text.set("Grid");require(take(check.snapshot()).checked,"An unrelated update reset user selection");
    scope->backend_0.checked.set(true);scope->backend_0.checked.set(false);
    require(!take(check.snapshot()).checked&&scope->backend_0.events.size()==1,"Programmatic update emitted a user callback");
    good(check.focus());good(state->input({.kind=ii::gui::WidgetInputKind::key_down,.key=ii::gui::WidgetKey::space}));
    good(state->input({.kind=ii::gui::WidgetInputKind::key_up,.key=ii::gui::WidgetKey::space}));
    require(take(check.snapshot()).checked&&scope->backend_0.events==std::vector<bool>({true,true}),"Keyboard activation failed");
    scope->backend_0.enabled.set(false);good(check.activate());
    require(!take(check.snapshot()).enabled&&scope->backend_0.events.size()==2&&
            has_inner_shadow(take(state->drawing_snapshot({640,480})),take(check.snapshot()).bounds),
            "Disabled checkbox admitted activation or lost its inner shadow");
    scope->backend_0.useAsset.set(true);
    visual=ink(take(state->drawing_snapshot({640,480})),take(check.snapshot()).bounds);
    auto disabled_asset=take(lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("checkboxCheckedDisabled",68));
    require(visual.mark&&visual.mark==disabled_asset&&!visual.tint,"Disabled checkbox did not use its authored full asset");
    scope->backend_0.useAsset.set(false);
    scope->backend_0.enabled.set(true);scope->backend_0.size.set(0);
    require(!take(graph->failures()).empty()&&take(check.snapshot()).checked,"Invalid metrics changed retained state");
    scope->backend_0.size.set(17);require(take(graph->failures()).empty(),"Checkbox did not recover from invalid metrics");
    auto bounds=take(check.snapshot()).bounds;
    pointer(ii::gui::WidgetInputKind::pointer_down,bounds.x+1,bounds.y+9);
    pointer(ii::gui::WidgetInputKind::pointer_up,bounds.x-1,bounds.y+9);
    require(take(check.snapshot()).checked&&scope->backend_0.events.size()==2,"Outside release activated checkbox");
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=bounds.x+8,.y=bounds.y+9,.device=ii::gui::PointerDevice::touch}));
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=bounds.x+8,.y=bounds.y+9,.device=ii::gui::PointerDevice::touch}));
    require(!take(check.snapshot()).checked&&scope->backend_0.events==std::vector<bool>({true,true,false}),"Touch activation failed");
    std::weak_ptr<View::Scope> weak=scope;good(host.clear_content());scope.reset();graph.reset();
    require(weak.expired()&&!check.snapshot(),"Retired checkbox retained its scope");state->close();
}
void retirement(){
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{320,200}));ii::gui::Widget check;
    std::optional<CheckBoxView> component;int callbacks=0;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets)->ii::core::Result<void>{
        auto item=CheckBoxView::create(widgets,{},"check",{},[&](bool){++callbacks;good(host.clear_content());});
        if(!item)return std::unexpected(item.error());check=item->root();component=*item;return {};
    }));
    require(!take(component->options()).checked,"Checkbox options lost the initial state");
    bool rejected=false;std::thread worker([&]{lvui::CheckBoxConfiguration value;value.checked=true;const auto result=component->set_options(value);
        const auto read=component->options();rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor&&
            !read&&read.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected&&!take(check.snapshot()).checked,"Foreign thread changed checkbox state");
    good(check.activate());require(callbacks==1&&!check.snapshot(),"Callback could not retire its generation safely");
    auto closed=component->set_options(lvui::CheckBoxConfiguration{});
    require(!closed&&closed.error().code==ii::core::ErrorCode::closed,"Retired checkbox remained writable");
    auto read=component->options();require(!read&&read.error().code==ii::core::ErrorCode::closed,"Retired checkbox options remained readable");
    component.reset();state->close();
}
void explicit_checked_update(){
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{320,200}));
    std::optional<CheckBoxView> check;int changes=0;
    good(host.set_content([&](auto& widgets)->ii::core::Result<void>{
        auto result=CheckBoxView::create(widgets,{},"check",{},[&](bool){++changes;});
        if(!result)return std::unexpected(result.error());check=*result;return {};
    }));
    good(check->root().activate());require(take(check->options()).checked&&changes==1,"User check was not reflected in options");
    const auto bounds=take(check->root().snapshot()).bounds;
    const auto mark=centered_mark(take(state->drawing_snapshot({320,200})),bounds);
    require(mark&&near(mark->bounds.width,17),"A host without a frame scheduler did not settle the checked asset");
    good(check->set_checked(false));
    require(!take(check->options()).checked&&!take(check->root().snapshot()).checked&&changes==1,
            "Explicitly restoring the original checked value was ignored or emitted a callback");
    good(host.clear_content());require(!check->set_checked(true),"Retired checkbox accepted an explicit check");state->close();
}
void animation(){
    using namespace std::chrono_literals;using namespace ii::gui;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"host",{320,200}));std::optional<CheckBoxView> box;
    lvui::CheckBoxConfiguration c;c.checked=true;c.use_figma_checked_assets=false;c.motion.duration=200ms;c.motion.curve=lvui::MotionCurve::linear;
    c.palette.unchecked={.8,.6,.4,.8};c.palette.checked={.4,.3,.2,.8};int callbacks=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{auto item=CheckBoxView::create(builder,{},"check",c,[&](bool){++callbacks;});
        if(!item)return std::unexpected(item.error());box=*item;return {};}));
    auto widget=box->root();const auto bounds=take(widget.snapshot()).bounds;(void)take(tree->drawing_snapshot({320,200}));
    c.checked=false;good(box->set_options(c));const auto anchor=ii::runtime::Clock::now()+1h;(void)frames.step(anchor);
    require(!take(widget.snapshot()).checked&&!centered_mark(take(tree->drawing_snapshot({320,200})),bounds),"Unchecked mark remained visible");
    c.checked=true;good(box->set_options(c));require(take(widget.snapshot()).checked,"Selection was not immediately available to accessibility");
    (void)frames.step(anchor+100ms);auto drawing=take(tree->drawing_snapshot({320,200}));auto mark=centered_mark(drawing,bounds);
    if(!mark||!near(mark->bounds.width,17*(.55+.45*.5))){
        std::string detail="Checkbox half-frame mark width="+std::to_string(mark?mark->bounds.width:0)+
            " widget="+std::to_string(bounds.x)+","+std::to_string(bounds.y)+","+std::to_string(bounds.width)+","+std::to_string(bounds.height);
        const auto inspect=[&](const auto& images){for(const auto& image:images)detail+=" image="+std::to_string(image.bounds.x)+","+
            std::to_string(image.bounds.y)+","+std::to_string(image.bounds.width)+","+std::to_string(image.bounds.height);};
        inspect(drawing.images);for(const auto& layer:drawing.overlays)inspect(layer.images);throw std::runtime_error(detail);
    }
    (void)frames.step(anchor+200ms);drawing=take(tree->drawing_snapshot({320,200}));mark=centered_mark(drawing,bounds);
    require(mark&&near(mark->bounds.width,17),"Checkbox mark did not settle");
    const auto linear=[](double x){return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+4,.y=bounds.y+9}));
    require(near(ink(take(tree->drawing_snapshot({320,200})),bounds).fill.red,linear(.4)),"Hover color jumped immediately");
    (void)frames.step(anchor+265ms);const auto hover=.4+(.4/1.08-.4)*.875;
    require(near(ink(take(tree->drawing_snapshot({320,200})),bounds).fill.red,linear(hover)),"Hover did not animate from the previous color");
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+bounds.width+5,.y=bounds.y+bounds.height+5}));
    require(near(ink(take(tree->drawing_snapshot({320,200})),bounds).fill.red,linear(hover)),"Hover exit jumped from its current color");
    (void)frames.step(anchor+330ms);const auto exit=hover+(.4-hover)*.875;
    require(near(ink(take(tree->drawing_snapshot({320,200})),bounds).fill.red,linear(exit)),"Hover exit did not retarget smoothly");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+4,.y=bounds.y+9,.buttons=1}));
    require(near(ink(take(tree->drawing_snapshot({320,200})),bounds).fill.red,linear(exit)),"Press color jumped from the visible value");
    (void)frames.step(anchor+395ms);const auto pressed=exit+(.4/1.2-exit)*.875;
    require(near(ink(take(tree->drawing_snapshot({320,200})),bounds).fill.red,linear(pressed)),"Press color did not animate");
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+bounds.width+5,.y=bounds.y+bounds.height+5,.buttons=0}));
    require(take(widget.snapshot()).checked&&callbacks==0,"Outside release changed checkbox selection");
    c.enabled=false;good(box->set_options(c));drawing=take(tree->drawing_snapshot({320,200}));
    require(!take(widget.snapshot()).enabled&&near(ink(drawing,bounds).fill.red,linear(c.palette.disabled_checked.red)),
            "Disabling did not settle checkbox color immediately");
    c.enabled=true;c.checked=false;c.motion.reduced_motion=true;good(box->set_options(c));
    require(!take(widget.snapshot()).checked&&!centered_mark(take(tree->drawing_snapshot({320,200})),bounds)&&callbacks==0,
            "Reduced-motion model update changed callbacks or retained a mark");
    c.motion.reduced_motion=false;c.motion.duration=360ms;c.motion.curve=lvui::MotionCurve::out_back;
    c.use_figma_checked_assets=true;c.frame_padding=0;good(box->set_options(c));const auto stock_bounds=take(widget.snapshot()).bounds;
    c.checked=true;good(box->set_options(c));(void)frames.step(anchor+575ms);drawing=take(tree->drawing_snapshot({320,200}));
    mark=centered_mark(drawing,stock_bounds,0);const auto original=take(lvui::native::authored_icon_image<ii::gui::CachedSvgDecoder>("checkboxCheckedEnabled",68));
    require(valid_canvas(drawing)&&mark&&mark->image==original&&!mark->tint&&mark->source.x>0&&mark->source.width<1,
            "Overshooting stock asset was not safely clipped to its indicator");
    good(widget.focus());drawing=take(tree->drawing_snapshot({320,200}));bool stock=false,focus=false;
    const auto inspect=[&](const auto& images){for(const auto& image:images){stock|=image.image==original;
        focus|=image.image!=original&&image.tint.has_value()&&near(image.bounds.width,17);}};
    inspect(drawing.images);for(const auto& layer:drawing.overlays)inspect(layer.images);
    require(valid_canvas(drawing)&&stock&&focus,"Stock asset covered the keyboard focus indicator");
    good(host.clear_content());(void)frames.step(anchor+2s);
    require(!widget.snapshot(),"A retired checkbox survived an active animation");tree->close();
}
void reentrant_frame_request(){
    using namespace std::chrono_literals;using namespace ii::gui;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);detail::WidgetFrameHooks hooks;
    std::optional<CheckBoxView> box;lvui::CheckBoxConfiguration newer;newer.motion_enabled=false;bool reenter=false;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};
    hooks.activity=[&](bool active){if(active&&std::exchange(reenter,false))good(box->set_options(newer));};
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(tree->content_host({},"host",{200,80}));
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{auto item=CheckBoxView::create(builder,{},"check",{});
        if(!item)return std::unexpected(item.error());box=*item;return {};}));
    auto older=newer;older.motion_enabled=true;older.checked=true;reenter=true;good(box->set_options(older));
    require(!reenter&&!take(box->options()).checked&&!take(box->root().snapshot()).checked,
            "A reentrant frame request let an older checkbox update overwrite the newer state");
    good(host.clear_content());tree->close();
}
void large_view(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,200}));Widget first,last;
    int external=0;auto port=std::make_shared<WidgetInteractionPort>([&](const auto& value){if(value.alive)++external;});
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{320,200}));
        for(int i=0;i<1025;++i){
            auto item=CheckBoxView::create(builder,root,"check"+std::to_string(i),{});
            if(!item)return std::unexpected(item.error());
            if(i==0){first=item->root();if(auto observed=first.observe_interaction(port);!observed)return observed;}
            last=item->root();
        }
        return {};
    }));
    (void)take(first.snapshot());require(external>0&&take(last.snapshot()).enabled,"A large checkbox view exhausted the external observer quota");
    good(host.clear_content());tree->close();
}
}
int main(){interaction();retirement();explicit_checked_update();animation();reentrant_frame_request();large_view();}
