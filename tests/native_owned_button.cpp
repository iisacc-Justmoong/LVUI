#include "NativeOwnedButton.generated.h"
#include <iiGUI/Motion.h>
#include <iiGUI/WidgetState.h>
#include <iiSDKCore/Runtime/FrameLoop.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <vector>
#include <type_traits>

namespace lvui::native {
using ButtonEvent=ButtonEventValue<ii::gui::Widget>;
using ButtonMethod=ButtonMethodValue<ButtonEvent>;
using ButtonMethodRegistry=ButtonMethodRegistryValue<ButtonEvent>;
using ButtonInput=ButtonInputValue<ii::gui::RasterImage>;
using ButtonModel=ii::gui::BasicButton<ButtonHostModelPolicy>;
using NativeButton=ButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>;
}
static_assert(std::is_same_v<lvui::native::NativeButton,
    ii::gui::BasicNativeButton<lvui::native::ButtonHostPolicy<lvui::native::ButtonModel,ii::gui::CachedSvgDecoder,ii::gui::Widget>>>);

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
bool has_title_color(const ii::gui::CanvasFrame& frame,std::string_view text){
    const auto in=[&](const auto& runs){return std::ranges::any_of(runs,[&](const auto& run){
        return run.text==text&&near(run.color.red,1)&&near(run.color.green,1)&&
               near(run.color.blue,1)&&near(run.color.alpha,.9);});};
    return in(frame.texts)||std::ranges::any_of(frame.overlays,[&](const auto& layer){return in(layer.texts);});
}
bool has_icon(const ii::gui::CanvasFrame& frame,const std::shared_ptr<const ii::gui::RasterImage>& image){
    const auto in=[&](const auto& runs){return std::ranges::any_of(runs,[&](const auto& run){return run.image==image&&near(run.bounds.width,18)&&near(run.bounds.height,18);});};
    if(in(frame.images))return true;
    return std::ranges::any_of(frame.overlays,[&](const auto& layer){return in(layer.images);});
}
void native_options(){
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"options-host",{320,200}));
    std::optional<lvui::native::NativeButton> button;
    lvui::native::ButtonInput authored;authored.configuration.text="Initial";
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=lvui::native::NativeButton::create(builder,{},"button",authored);
        if(!created)return std::unexpected(created.error());button=*created;return {};
    }));
    require(take(button->options())==authored,"Button did not expose authored options");
    auto changed=take(button->options());changed.configuration.text="Updated";good(button->set_options(changed));
    require(take(button->options())==changed&&take(button->root().snapshot()).text=="Updated",
            "Button options did not follow an update");
    changed.configuration.tone=lvui::ButtonTone::destructive;changed.configuration.motion_enabled=false;
    changed.configuration.palette.danger={.6,.3,.2,1};
    changed.configuration.palette.danger_reference=changed.configuration.palette.danger;
    changed.configuration.palette.danger_hover_default={.125,.25,.5,.75};
    changed.configuration.palette.danger_pressed_default={.75,.5,.25,.125};
    good(button->set_options(changed));(void)take(state->drawing_snapshot({320,200}));
    const auto bounds=take(button->root().snapshot()).bounds;
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_move,
        .x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    auto fill=background(take(state->drawing_snapshot({320,200})),bounds);
    require(near(fill.red,linear(.125))&&near(fill.alpha,.75),"Themed Button did not paint its hover token");
    auto custom=take(button->options());custom.configuration.palette.danger={.4,.3,.2,1};
    good(button->set_options(custom));fill=background(take(state->drawing_snapshot({320,200})),bounds);
    require(near(fill.red,linear(.4/1.12))&&near(fill.alpha,1),
        "Updating Button options retained an obsolete themed danger hover color");
    auto selected=take(button->options());selected.configuration.tone=lvui::ButtonTone::normal;
    selected.configuration.checkable=true;selected.configuration.checked=true;
    selected.configuration.palette.primary={.8,.2,.1,1};
    selected.configuration.palette.selected_reference=selected.configuration.palette.primary;
    selected.configuration.palette.selected_default={.1,.2,.3,1};
    selected.configuration.palette.selected_surface={.4,.5,.6,1};
    good(button->set_options(selected));fill=background(take(state->drawing_snapshot({320,200})),bounds);
    require(near(fill.red,linear(.1)),"Button host ignored the injected selected token");
    selected.configuration.palette.primary={1,.2,.1,1};
    good(button->set_options(selected));fill=background(take(state->drawing_snapshot({320,200})),bounds);
    require(near(fill.red,linear(.4*.75+.25)),"Button host ignored the injected selection surface");
    bool rejected=false;std::thread worker([&]{auto value=button->options();rejected=!value&&value.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread read button options");
    good(host.clear_content());auto closed=button->options();
    require(!closed&&closed.error().code==ii::core::ErrorCode::closed,"Retired button options remained readable");state->close();
}
void native_input_events(){
    using namespace ii::gui;
    auto state=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"events-host",{200,100}));
    lvui::native::ButtonInput options;options.configuration.width=100;options.configuration.height=30;
    std::optional<lvui::native::NativeButton> button;std::vector<std::string> events;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=take(builder.absolute_stack({},"events-root",{200,100}));
        button=take(lvui::native::NativeButton::create(builder,root,"button",options,[&]{events.emplace_back("clicked");}));
        return button->root().set_frame({20,20,100,30});
    }));
    good(button->set_events({.pressed=[&]{events.emplace_back("pressed");},
        .released=[&]{events.emplace_back("released");},.canceled=[&]{events.emplace_back("canceled");}}));
    (void)take(state->drawing_snapshot({200,100}));
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=30}));
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=30,.y=30}));
    require(events==std::vector<std::string>{"pressed","released","clicked"},"Pointer button event order is wrong");
    events.clear();good(state->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=30}));
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=190,.y=90}));
    require(events==std::vector<std::string>{"pressed","released"},"Outside release incorrectly clicked");
    for(auto key:{WidgetKey::space,WidgetKey::enter}){
        events.clear();good(button->root().focus());
        good(state->input({.kind=WidgetInputKind::key_down,.key=key}));
        good(state->input({.kind=WidgetInputKind::key_up,.key=key}));
        require(events==std::vector<std::string>{"pressed","released","clicked"},"Keyboard button event order is wrong");
    }
    events.clear();good(state->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=30}));
    good(state->input({.kind=WidgetInputKind::focus_lost}));
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=30,.y=30}));
    require(events==std::vector<std::string>{"pressed","canceled"},"Focus loss did not cancel the button");
    events.clear();good(button->root().focus());
    good(state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));
    good(state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    good(state->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::space}));
    if(events!=std::vector<std::string>{"pressed","canceled"}){
        std::string observed;for(const auto& event:events)observed+=" "+event;
        throw std::runtime_error("Escape did not cancel the button:"+observed);
    }
    events.clear();good(button->root().activate());
    require(events==std::vector<std::string>{"clicked"},"Programmatic activation emitted input events");
    options.configuration.enabled=false;good(button->set_options(options));events.clear();
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=30}));
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=30,.y=30}));good(button->root().activate());
    require(events.empty(),"Disabled button delivered events");
    options.configuration.enabled=true;good(button->set_options(options));
    good(button->set_events({.pressed=[&]{events.emplace_back("old press");good(button->set_events({
        .released=[&]{events.emplace_back("new release");}}));}}));
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=30}));
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=30,.y=30}));
    require(events==std::vector<std::string>{"old press","new release","clicked"},"Reentrant event replacement failed");
    bool rejected=false;std::thread worker([&]{auto result=button->set_events({});
        rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Button accepted events from a foreign thread");
    events.clear();good(button->set_events({.released=[&]{events.emplace_back("retired");good(host.clear_content());}}));
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=30}));
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=30,.y=30}));
    require(events==std::vector<std::string>{"retired"},"Retirement delivered a stale click");
    auto closed=button->set_events({});require(!closed&&closed.error().code==ii::core::ErrorCode::closed,
        "Retired button accepted events");state->close();
}
void native_input_methods(){
    using namespace ii::gui;using lvui::native::ButtonEvent;using lvui::native::ButtonMethod;
    struct Invoke final {std::vector<std::string>* events;void invoke(const ButtonEvent&)const{events->emplace_back("invoke");}};
    struct Trigger final {std::vector<std::string>* events;void trigger()const{events->emplace_back("trigger");}};
    struct Shared final {int calls=0;void invoke(const ButtonEvent&){++calls;}};
    std::vector<std::string> events;
    auto invoke=lvui::native::collect_button_methods<ButtonEvent>(Invoke{&events});
    auto trigger=lvui::native::collect_button_methods<ButtonEvent>(Trigger{&events});
    std::vector<std::vector<std::optional<ButtonMethod>>> nested={{{},ButtonMethod{[&](const auto&){events.emplace_back("nested");}}}};
    auto flattened=lvui::native::collect_button_methods<ButtonEvent>(nested);
    require(invoke.size()==1&&trigger.size()==1&&flattened.size()==1,"Button methods did not flatten callable values");
    require(lvui::native::collect_button_methods<ButtonEvent>(std::function<void()>{}).empty(),"Empty no-argument callback was retained");
    auto shared=std::make_shared<Shared>();std::weak_ptr<Shared> weak_shared=shared;
    auto retained=lvui::native::collect_button_methods<ButtonEvent>(shared);shared.reset();
    require(retained.size()==1&&!weak_shared.expired(),"Shared button method did not retain its target");
    retained.front()(ButtonEvent{});require(weak_shared.lock()->calls==1,"Shared method invoked a copied target");
    retained.clear();require(weak_shared.expired(),"Released shared method retained its target");
    bool overflow=false;try{(void)lvui::native::collect_button_methods<ButtonEvent>(std::vector<ButtonMethod>(257,[](const auto&){}));}
    catch(const std::length_error&){overflow=true;}require(overflow,"An excessive method list was accepted");
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"methods-host",{240,100}));
    lvui::native::ButtonInput options;options.configuration.width=100;options.configuration.height=30;
    options.configuration.tone=lvui::ButtonTone::normal;std::optional<lvui::native::NativeButton> button;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{240,100}));
        button=take(lvui::native::NativeButton::create(builder,root,"button",options,[&]{events.emplace_back("clicked");}));
        return button->root().set_frame({20,20,100,30});
    }));
    require(!take(button->has_methods()),"Empty button registry was reported as populated");
    good(button->set_method([&](const ButtonEvent& event){
        require(event.source.snapshot().has_value()&&event.trigger=="clicked"&&event.enabled&&event.effective_enabled&&
                event.tone==lvui::ButtonTone::normal,"Button method payload is wrong");events.emplace_back("method");}));
    std::vector<ButtonMethod> all;all.insert(all.end(),invoke.begin(),invoke.end());
    all.insert(all.end(),trigger.begin(),trigger.end());all.insert(all.end(),flattened.begin(),flattened.end());
    good(button->set_methods(all));require(take(button->has_methods()),"Button registry lost its methods");
    good(button->set_methods(std::vector<ButtonMethod>(257)));
    require(take(button->has_methods()),"Empty method entries removed the separately registered method");
    good(button->set_methods(all));
    (void)take(tree->drawing_snapshot({240,100}));good(button->root().activate());
    require(events==std::vector<std::string>{"method","invoke","trigger","nested","clicked"},"Button methods ran in the wrong order");
    require(!button->set_methods(std::vector<ButtonMethod>(257,[](const auto&){})),"Excess methods replaced the registry");
    events.clear();good(button->root().activate());require(events==std::vector<std::string>{"method","invoke","trigger","nested","clicked"},
        "Rejected methods changed the button registry");
    bool rejected=false;std::thread worker([&]{auto result=button->set_method({});rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    worker.join();require(rejected,"A foreign thread replaced a button method");
    events.clear();good(button->set_method([&](const auto&){events.emplace_back("disabled");options.configuration.enabled=false;good(button->set_options(options));}));
    good(button->root().activate());require(events==std::vector<std::string>{"disabled"},"A disabled button delivered stale methods or click");
    options.configuration.enabled=true;good(button->set_options(options));events.clear();
    good(button->set_method([&](const auto&){events.emplace_back("retired");good(host.clear_content());}));
    good(button->root().activate());require(events==std::vector<std::string>{"retired"},"A retired button delivered stale methods or click");
    auto closed=button->set_method({});require(!closed&&closed.error().code==ii::core::ErrorCode::closed,
        "Retired button accepted a method");tree->close();
}
void button_color_motion(){
    using namespace std::chrono_literals;using namespace ii::gui;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);
    auto preferences=std::make_shared<MotionPreferences>();detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};hooks.preferences=preferences;
    auto state=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(state->content_host({},"button-motion-host",{200,100}));
    lvui::native::ButtonInput options;options.configuration.width=100;options.configuration.height=30;
    options.configuration.tone=lvui::ButtonTone::normal;
    options.configuration.palette.normal={.2,.3,.4,.4};options.configuration.palette.normal_hover={.8,.6,.4,.8};
    options.configuration.palette.normal_pressed={.9,.2,.1,1};options.configuration.palette.selected=lvui::StyleColor{.4,.8,.2,.6};
    std::optional<lvui::native::NativeButton> button;int activations=0;bool retire_on_action=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"button-motion-root",{200,100});if(!root)return std::unexpected(root.error());
        auto created=lvui::native::NativeButton::create(builder,*root,"button",options,[&]{
            ++activations;if(retire_on_action)good(host.clear_content());
        });
        if(!created)return std::unexpected(created.error());button=*created;return button->root().set_frame({20,20,100,30});
    }));
    const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    const auto fill=[&]{return background(take(state->drawing_snapshot({200,100})),take(button->root().snapshot()).bounds);};
    const auto idle=fill();require(near(idle.red,linear(.2))&&near(idle.alpha,.4),"Button motion began with the wrong idle color");
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=40,.y=30}));
    require(near(fill().red,idle.red),"Button hover jumped instead of starting a color transition");
    const auto anchor=ii::runtime::Clock::now()+1h;(void)frames.step(anchor);
    require(near(fill().red,linear(.8))&&near(fill().alpha,.8),"Button hover did not settle on its target color");
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=180,.y=80}));
    require(near(fill().red,linear(.8)),"Button exit jumped instead of animating");
    (void)frames.step(anchor+50ms);const auto halfway=fill();
    constexpr auto ease_out_half=.6846431874274606;
    require(near(halfway.red,linear(.8+(.2-.8)*ease_out_half))&&
            near(halfway.green,linear(.6+(.3-.6)*ease_out_half))&&
            near(halfway.alpha,.8+(.4-.8)*ease_out_half),
            "Button exit did not follow the 100ms Figma ease-out approximation");
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=40,.y=30}));
    require(near(fill().red,halfway.red),"Button color reversal did not begin at the displayed color");
    (void)frames.step(anchor+130ms);require(fill().red>halfway.red&&fill().red<linear(.8),"Button reversal did not progress");
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=30}));
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=30}));
    require(activations==1,"Button activation waited for its color transition");
    good(preferences->set_reduced_motion(true));(void)frames.step(anchor+131ms);
    require(near(fill().red,linear(.8)),"Reduced motion did not settle an active button transition");
    good(button->root().set_checked(true));
    require(near(fill().red,linear(.4)),"Direct host selection displayed a stale animated background");
    good(preferences->set_reduced_motion(false));
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=30}));
    require(near(fill().red,linear(.4)),"Press after direct selection jumped from the visible color");
    good(preferences->set_reduced_motion(true));(void)frames.step(anchor+132ms);
    require(near(fill().red,linear(.9)),"Reduced motion did not settle the pressed color");
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=30}));
    require(activations==2,"Button activation was lost while using reduced motion");
    good(button->root().set_checked(false));
    require(near(fill().red,linear(.8)),"Clearing direct host selection displayed a stale background");
    good(preferences->set_reduced_motion(false));good(preferences->set_speed(2));
    options.configuration.checked=true;good(button->set_options(options));
    require(near(fill().red,linear(.8)),"Checked color jumped instead of transitioning");
    (void)frames.step(anchor+196ms);require(near(fill().red,linear(.4)),"Button ignored shared motion speed or checked color");
    options.configuration.palette.selected=lvui::StyleColor{.6,.1,.2,.7};good(button->set_options(options));
    require(near(fill().red,linear(.4)),"Palette update jumped from the displayed color");
    (void)frames.step(anchor+261ms);require(near(fill().red,linear(.6)),"Button did not animate a reactive palette change");
    options.configuration.checkable=true;options.configuration.checked=false;good(button->set_options(options));
    (void)frames.step(anchor+326ms);require(near(fill().red,linear(.8)),"Unchecked button did not return to its hover color");
    good(button->root().activate());
    require(activations==3&&near(fill().red,linear(.8)),"Checkable activation did not start at the displayed color");
    (void)frames.step(anchor+391ms);require(near(fill().red,linear(.6)),"Checkable activation did not animate its selected color");
    options.configuration.enabled=false;good(button->set_options(options));
    require(near(fill().red,linear(options.configuration.palette.disabled_background.red)),"Disabled button kept an animated background");
    options.configuration.enabled=true;options.configuration.checked=false;good(button->set_options(options));
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=180,.y=80}));
    retire_on_action=true;good(button->root().activate());
    require(activations==4&&!button->root().snapshot(),"Button action did not retire its own view safely");
    (void)frames.step(anchor+500ms);state->close();
}
void button_local_motion(){
    using namespace std::chrono_literals;using namespace ii::gui;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);
    auto preferences=std::make_shared<MotionPreferences>();detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};hooks.preferences=preferences;
    auto state=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(state->content_host({},"local-motion-host",{240,100}));
    lvui::native::ButtonInput one,two;one.configuration.width=80;one.configuration.height=24;
    one.configuration.tone=lvui::ButtonTone::normal;one.configuration.palette.normal={.2,.3,.4,1};two=one;
    std::optional<lvui::native::NativeButton> first,second;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.absolute_stack({},"local-motion-root",{240,100});if(!root)return std::unexpected(root.error());
        auto a=lvui::native::NativeButton::create(builder,*root,"first",one);if(!a)return std::unexpected(a.error());first=*a;
        auto b=lvui::native::NativeButton::create(builder,*root,"second",two);if(!b)return std::unexpected(b.error());second=*b;
        if(auto placed=first->root().set_frame({10,10,80,24});!placed)return placed;
        return second->root().set_frame({120,10,80,24});
    }));
    const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    const auto fill=[&](const auto& item){return background(take(state->drawing_snapshot({240,100})),take(item->root().snapshot()).bounds);};
    require(one.configuration.motion_enabled&&two.configuration.motion_enabled,"Button motion must default to enabled");
    (void)fill(first);(void)fill(second);
    one.configuration.palette.normal.red=.8;two.configuration.palette.normal.red=.8;
    good(first->set_options(one));good(second->set_options(two));
    const auto anchor=ii::runtime::Clock::now()+1h;(void)frames.step(anchor);
    require(near(fill(first).red,linear(.8))&&near(fill(second).red,linear(.8)),"Initial transitions did not settle");
    one.configuration.palette.normal.red=.2;two.configuration.palette.normal.red=.2;
    good(first->set_options(one));good(second->set_options(two));(void)frames.step(anchor+65ms);
    require(near(fill(first).red,linear(.275))&&near(fill(second).red,linear(.275)),"Buttons did not animate independently");
    one.configuration.motion_enabled=false;good(first->set_options(one));
    require(near(fill(first).red,linear(.2))&&near(fill(second).red,linear(.275)),
            "Local motion opt-out failed to settle its own button or changed another button");
    one.configuration.palette.normal.red=.8;good(first->set_options(one));
    require(near(fill(first).red,linear(.8)),"Locally disabled motion did not apply the next target immediately");
    one.configuration.motion_enabled=true;one.configuration.palette.normal.red=.2;good(first->set_options(one));
    require(near(fill(first).red,linear(.8)),"Reenabled motion did not resume on the next change");
    (void)frames.step(anchor+130ms);
    require(near(fill(first).red,linear(.275)),"Reenabled local motion did not advance");
    good(preferences->set_reduced_motion(true));(void)frames.step(anchor+131ms);
    require(near(fill(first).red,linear(.2))&&near(fill(second).red,linear(.2)),
            "Global reduced motion did not override enabled local motion");
    good(preferences->set_reduced_motion(false));good(preferences->set_enabled(false));
    one.configuration.palette.normal.red=.8;good(first->set_options(one));
    require(near(fill(first).red,linear(.8)),"Global disabled motion did not override enabled local motion");
    good(preferences->set_enabled(true));one.configuration.palette.normal.red=.2;good(first->set_options(one));
    require(near(fill(first).red,linear(.8)),"Restoring global motion did not resume the next transition");
    (void)frames.step(anchor+196ms);require(near(fill(first).red,linear(.275)),"Restored global motion did not advance");
    good(host.clear_content());state->close();
}
void button_figma_state_timing(){
    using namespace std::chrono_literals;using namespace ii::gui;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);
    auto preferences=std::make_shared<MotionPreferences>();detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};hooks.preferences=preferences;
    auto state=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(state->content_host({},"figma-timing-host",{200,100}));
    lvui::native::ButtonInput options;options.configuration.width=100;options.configuration.height=30;
    options.configuration.tone=lvui::ButtonTone::normal;
    options.configuration.palette.normal={.2,.3,.4,1};options.configuration.palette.normal_hover={.8,.6,.4,1};
    options.configuration.palette.normal_pressed={.5,.2,.1,1};
    std::optional<lvui::native::NativeButton> button;int clicks=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=take(builder.absolute_stack({},"root",{200,100}));
        button=take(lvui::native::NativeButton::create(builder,root,"button",options,[&]{++clicks;}));
        return button->root().set_frame({20,20,100,30});
    }));
    const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    const auto red=[&]{return background(take(state->drawing_snapshot({200,100})),take(button->root().snapshot()).bounds).red;};
    constexpr auto halfway=.6846431874274606;
    (void)red();good(state->input({.kind=WidgetInputKind::pointer_move,.x=40,.y=30}));
    const auto anchor=ii::runtime::Clock::now()+1h;(void)frames.step(anchor);
    require(near(red(),linear(.8)),"Button hover did not settle");
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=30}));
    (void)frames.step(anchor+30ms);
    require(near(red(),linear(.8+(.5-.8)*halfway)),"Button press did not use its 60ms transition");
    (void)frames.step(anchor+60ms);require(near(red(),linear(.5)),"Button press did not settle at 60ms");
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=30}));
    require(clicks==1,"Button click was delayed until the animation ended");
    (void)frames.step(anchor+100ms);
    require(near(red(),linear(.5+(.8-.5)*halfway)),"Button release did not use its 80ms transition");
    (void)frames.step(anchor+140ms);require(near(red(),linear(.8)),"Button release did not settle at 80ms");
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=180,.y=80}));
    (void)frames.step(anchor+200ms);
    require(near(red(),linear(.8+(.2-.8)*halfway)),"Leaving immediately after release did not use 120ms");
    (void)frames.step(anchor+260ms);require(near(red(),linear(.2)),"Post-release exit did not settle at 120ms");
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=40,.y=30}));
    (void)frames.step(anchor+360ms);
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=30}));
    (void)frames.step(anchor+420ms);require(near(red(),linear(.5)),"Second pointer press did not settle");
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=180,.y=80}));
    (void)frames.step(anchor+460ms);
    require(near(red(),linear(.5+(.2-.5)*halfway)),"Leaving a held pointer did not begin the 80ms cancellation");
    (void)frames.step(anchor+500ms);require(near(red(),linear(.2)),"Held pointer exit did not settle");
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=40,.y=30}));
    (void)frames.step(anchor+600ms);require(near(red(),linear(.8)),"Reentering a canceled press did not show hover");
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=30}));
    require(clicks==2,"Pointer reentry changed the native click contract");
    good(button->root().focus());
    good(state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::space}));
    (void)frames.step(anchor+660ms);require(near(red(),linear(.5)),"Keyboard press did not settle");
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=180,.y=80}));
    (void)frames.step(anchor+760ms);require(near(red(),linear(.5)),"Pointer exit canceled a keyboard press");
    good(state->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::space}));
    require(clicks==3,"Keyboard activation was lost after pointer exit");
    good(host.clear_content());state->close();
}
void generated_button_local_motion(){
    using namespace std::chrono_literals;using namespace ii::gui;using View=lvui_generated::NativeOwnedButton;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);
    auto preferences=std::make_shared<MotionPreferences>();detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};hooks.preferences=preferences;
    auto state=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(state->content_host({},"generated-motion-host",{640,480}));
    auto image=take(ii::gui::rgba_image(1,1,std::vector<std::uint8_t>{255,255,255,255}));
    std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](WidgetBuilder& builder){return View::build(builder,{},[&](auto& value){value.backend_0.icon.set(image);},&scope);}));
    auto button=find(state,"/save");const auto bounds=take(button.snapshot()).bounds;
    const auto fill=[&]{return background(take(state->drawing_snapshot({640,480})),bounds);};
    const auto same=[](auto a,auto b){return near(a.red,b.red)&&near(a.green,b.green)&&near(a.blue,b.blue)&&near(a.alpha,b.alpha);};
    const auto idle=fill();
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    const auto anchor=ii::runtime::Clock::now()+1h;(void)frames.step(anchor);const auto hover=fill();
    require(!same(idle,hover),"Generated button did not change its hover color");
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=630,.y=470}));(void)frames.step(anchor+65ms);
    require(!same(fill(),idle)&&!same(fill(),hover),"Generated button did not begin its color transition");
    scope->backend_0.motion_enabled.set(false);
    require(same(fill(),idle),"Reactive motionEnabled did not settle the generated button");
    scope->backend_0.motion_enabled.set(true);
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2}));
    require(same(fill(),idle),"Generated button did not resume local motion");
    (void)frames.step(anchor+130ms);require(!same(fill(),idle)&&!same(fill(),hover),"Generated button did not resume animation");
    good(preferences->set_reduced_motion(true));(void)frames.step(anchor+131ms);
    require(same(fill(),hover),"Global reduced motion did not override generated motionEnabled");
    good(host.clear_content());scope.reset();state->close();
}
void button_focus_motion(){
    using namespace std::chrono_literals;using namespace ii::gui;
    ii::runtime::EventLoop events;ii::runtime::FrameLoop frames(events);
    auto preferences=std::make_shared<MotionPreferences>();detail::WidgetFrameHooks hooks;
    hooks.frames=[&]()->ii::core::Result<ii::runtime::FrameLoop*>{return &frames;};hooks.preferences=preferences;
    auto state=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();},WidgetAppearance{},std::move(hooks));
    auto host=take(state->content_host({},"focus-motion-host",{250,100}));
    lvui::native::ButtonInput first_options,second_options;
    first_options.configuration.width=second_options.configuration.width=80;
    first_options.configuration.height=second_options.configuration.height=30;
    first_options.configuration.palette.primary=second_options.configuration.palette.primary={1,.2,.3,1};
    std::optional<lvui::native::NativeButton> first,second;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=take(builder.absolute_stack({},"focus-motion-root",{250,100}));
        first=take(lvui::native::NativeButton::create(builder,root,"first",first_options));
        second=take(lvui::native::NativeButton::create(builder,root,"second",second_options));
        good(first->root().set_frame({20,20,80,30}));return second->root().set_frame({140,20,80,30});
    }));
    const auto ring=[&](bool left)->std::optional<Panel>{
        auto frame=take(state->drawing_snapshot({250,100}));
        for(const auto& layer:frame.overlays)for(const auto& panel:layer.panels)
            if(panel.stroke_width>0 && ((panel.bounds.x+panel.bounds.width/2<120)==left))return panel;
        return std::nullopt;
    };
    (void)ring(true);good(first->root().focus());
    require(!ring(true),"Focus ring skipped its initial transparent state");
    const auto anchor=ii::runtime::Clock::now()+1h;(void)frames.step(anchor);
    const auto full=ring(true);
    require(bool(full),"Focus ring disappeared before settling");
    if(!near(full->top.alpha,1)||!near(full->bounds.width,86))
        throw std::runtime_error("Focus ring settled with opacity="+std::to_string(full->top.alpha)+
                                 " width="+std::to_string(full->bounds.width));
    good(second->root().focus());(void)frames.step(anchor+360ms);
    require(!ring(true)&&ring(false)&&near(ring(false)->top.alpha,1),"Focus transfer did not settle");
    good(first->root().focus());
    require(!ring(true)&&ring(false)&&near(ring(false)->top.alpha,1),"Focus transfer jumped before its next frame");
    (void)frames.step(anchor+440ms);
    const auto entering=ring(true),leaving=ring(false);
    require(entering&&leaving&&near(entering->top.alpha,.875)&&near(leaving->top.alpha,.125),
            "Focus opacity did not use the 160ms OutCubic transition");
    const double t=80./360.,u=t-1,progress=1+2.45*u*u*u+1.45*u*u;
    require(near(entering->bounds.width,86*(.96+.04*progress))&&
            near(leaving->bounds.width,86*(1-.04*progress)),
            "Focus size did not use the 360ms OutBack transition");
    good(second->root().focus());
    require(ring(true)&&ring(false)&&near(ring(true)->top.alpha,entering->top.alpha)&&
            near(ring(false)->top.alpha,leaving->top.alpha),"Focus reversal jumped from the displayed opacity");
    (void)frames.step(anchor+520ms);
    require(ring(true)&&ring(false)&&near(ring(true)->top.alpha,.109375)&&near(ring(false)->top.alpha,.890625),
            "Focus reversal did not continue from both displayed opacities");
    good(preferences->set_reduced_motion(true));(void)frames.step(anchor+521ms);
    require(!ring(true)&&ring(false)&&near(ring(false)->top.alpha,1)&&near(ring(false)->bounds.width,86),
            "Reduced motion did not settle both focus rings");
    good(preferences->set_reduced_motion(false));good(first->root().focus());
    second_options.configuration.motion_enabled=false;good(second->set_options(second_options));
    require(!ring(true)&&!ring(false),"Local motion opt-out did not settle its own exiting focus ring");
    (void)frames.step(anchor+601ms);
    require(ring(true)&&near(ring(true)->top.alpha,.875),"Local motion opt-out changed another button's focus animation");
    second_options.configuration.motion_enabled=true;good(second->set_options(second_options));
    good(second->root().focus());(void)frames.step(anchor+602ms);
    good(preferences->set_enabled(false));(void)frames.step(anchor+603ms);
    require(!ring(true)&&ring(false)&&near(ring(false)->top.alpha,1),
            "Disabled global motion did not settle the focus transition");
    good(preferences->set_enabled(true));good(first->root().set_frame({0,0,8192,8192}));
    good(first->root().focus());(void)frames.step(anchor+723ms);
    require(valid_canvas(take(state->drawing_snapshot({250,100}))),
            "Focus scale overshoot produced an invalid maximum-size drawing");
    good(host.clear_content());state->close();

    auto no_frames=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto plain_host=take(no_frames->content_host({},"plain-focus-host",{120,60}));
    std::optional<lvui::native::NativeButton> plain;
    good(plain_host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        plain=take(lvui::native::NativeButton::create(builder,{},"plain-focus",first_options));return {};
    }));
    good(plain->root().focus());
    const auto plain_frame=take(no_frames->drawing_snapshot({120,60}));
    const auto focused=[](const auto& panels){return std::ranges::any_of(panels,[](const auto& p){return p.stroke_width>0&&near(p.top.alpha,1);});};
    require(focused(plain_frame.panels)||std::ranges::any_of(plain_frame.overlays,[&](const auto& layer){return focused(layer.panels);}),
            "A published button without frame hooks did not immediately display focus");
    good(plain_host.clear_content());no_frames->close();
}
void button_focus_ring(){
    using namespace ii::gui;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto image=take(ii::gui::rgba_image(1,1,std::vector<std::uint8_t>{255,255,255,255}));
    std::optional<lvui::native::NativeButton> button,label;
    Widget parent;
    int clicks=0;
    lvui::native::ButtonInput options;
    options.configuration.icon_mode=true;options.configuration.has_icon=true;
    options.configuration.width=22;options.configuration.height=22;
    options.configuration.horizontal_padding=2;options.configuration.icon_size=18;
    options.icon=image;
    good(state->construct([&](WidgetBuilder& builder)->ii::core::Result<void>{
        parent=take(builder.absolute_stack({},"container",{100,80}));
        good(parent.set_clip_content(true));
        button=take(lvui::native::NativeButton::create(builder,parent,"icon",options,[&]{++clicks;}));
        good(button->root().set_frame({20,20,22,22}));
        lvui::native::ButtonInput label_options;label_options.configuration.text="Label";
        label=take(lvui::native::NativeButton::create(builder,parent,"label",label_options));
        return label->root().set_frame({20,50,56,22});
    }));
    auto focus_layer=[&](const CanvasFrame& frame)->std::optional<CanvasLayer>{
        for(const auto& layer:frame.overlays)
            if(std::ranges::any_of(layer.panels,[](const auto& panel){return near(panel.stroke_width,1.5);}))return layer;
        return std::nullopt;
    };
    require(!focus_layer(take(state->drawing_snapshot({120,100}))),"Unfocused icon button drew a focus ring");
    good(button->root().focus());
    auto layer=focus_layer(take(state->drawing_snapshot({120,100})));
    require(layer.has_value(),"Focused icon button did not draw its focus ring");
    const auto ring=std::ranges::find_if(layer->panels,[](const auto& panel){return near(panel.stroke_width,1.5);});
    require(ring!=layer->panels.end()&&ring->bounds==Rect{17,17,28,28}&&near(ring->radius,11),
            "Icon focus ring did not use the outside Figma geometry");
    require(!layer->clip || (layer->clip->x<=17 && layer->clip->y<=17 &&
            layer->clip->x+layer->clip->width>=45 && layer->clip->y+layer->clip->height>=45),
            "Icon focus ring was clipped to the button instead of its ancestor");
    good(label->root().focus());
    layer=focus_layer(take(state->drawing_snapshot({120,100})));
    require(layer.has_value(),"Focused label button did not draw its focus ring");
    const auto label_ring=std::ranges::find_if(layer->panels,[](const auto& panel){return near(panel.stroke_width,1.5);});
    require(label_ring!=layer->panels.end()&&label_ring->bounds==Rect{17,47,62,28}&&near(label_ring->radius,11),
            "Label focus ring did not use the outside Figma geometry");
    good(button->root().focus());
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=19,.y=30}));
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=19,.y=30}));
    require(clicks==0,"The focus ring enlarged the button hit target");
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=30,.y=30}));
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=30,.y=30}));
    require(clicks==1,"The button hit target changed with the focus ring");
    good(button->root().set_frame({0,0,22,22}));good(button->root().focus());
    layer=focus_layer(take(state->drawing_snapshot({120,100})));
    require(layer&&layer->clip&&layer->clip->x==0&&layer->clip->y==0,
            "Ancestor clipping did not apply to the outside focus ring");
    good(button->root().set_frame({0,0,8192,8192}));
    require(ii::gui::valid_canvas(take(state->drawing_snapshot({120,100}))),
            "Largest permitted button generated an invalid focus drawing");
    good(button->root().set_frame({20,20,1,1}));
    require(ii::gui::valid_canvas(take(state->drawing_snapshot({120,100}))),
            "Small button generated an invalid focus drawing");
    options.configuration.enabled=false;good(button->set_options(options));
    require(!focus_layer(take(state->drawing_snapshot({120,100}))),"Disabled icon button drew a focus ring");
    state->close();
}
}
int main(){
    native_options();
    native_input_events();
    native_input_methods();
    button_color_motion();
    button_figma_state_timing();
    button_local_motion();
    generated_button_local_motion();
    button_focus_motion();
    button_focus_ring();
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
    const auto normal=find(state,"/default-normal");
    const auto normal_fill=background(frame,take(normal.snapshot()).bounds);
    const auto linear_color=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    require(near(normal_fill.red,linear_color(lvui::figma_defaults::panel_surface_solid.red))&&
            near(normal_fill.green,linear_color(lvui::figma_defaults::panel_surface_solid.green))&&
            near(normal_fill.blue,linear_color(lvui::figma_defaults::panel_surface_solid.blue)),
            "Generated default button did not use the Figma solid surface");
    const auto danger=find(state,"/danger");const auto danger_bounds=take(danger.snapshot()).bounds;
    const auto dx=danger_bounds.x+danger_bounds.width/2,dy=danger_bounds.y+danger_bounds.height/2;
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=dx,.y=dy}));
    auto danger_fill=background(take(state->drawing_snapshot({640,480})),danger_bounds);
    require(near(danger_fill.red,linear_color(.1))&&near(danger_fill.green,linear_color(.6)),
            "Generated danger hover color was not applied");
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=dx,.y=dy}));
    scope->backend_0.danger_pressed.set({.7,.2,.4,1});
    danger_fill=background(take(state->drawing_snapshot({640,480})),danger_bounds);
    require(near(danger_fill.red,linear_color(.7))&&near(danger_fill.blue,linear_color(.4)),
            "Generated danger pressed color was not reactive");
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=dx,.y=dy}));
    require(has_text(frame,"Save",before.font_family),"Button text or resolved font was not painted");
    require(has_icon(frame,image),"Generated IconButton did not paint an 18px icon");
    require(has_title_color(frame,"Save"),"Generated button did not paint the Figma title text color");
    const auto x=before.bounds.x+before.bounds.width/2,y=before.bounds.y+before.bounds.height/2;
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=x,.y=y}));
    const auto hover=background(take(state->drawing_snapshot({640,480})),before.bounds);
    require(hover!=idle,"Hover did not update the LVUI button palette");
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=x,.y=y}));
    const auto pressed=background(take(state->drawing_snapshot({640,480})),before.bounds);
    require(pressed!=hover,"Press did not update the LVUI button palette");
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=x,.y=y}));
    require(scope->backend_0.saves==1,"Pointer activation did not invoke the action once");
    require(scope->backend_0.events==std::vector<std::string>{"pressed","released","method","after","last","clicked"},
            "Generated button did not deliver pointer events in order");
    scope->backend_0.events.clear();
    auto abstract=find(state,"/plain");
    require(!take(abstract.action_options()).enter_key_activation&&take(icon.action_options()).enter_key_activation,
            "Abstract and icon buttons have incorrect default Enter policies");
    scope->backend_0.enter_enabled.set(false);
    good(button.focus());
    good(state->input({.kind=ii::gui::WidgetInputKind::key_down,.key=ii::gui::WidgetKey::enter}));
    good(state->input({.kind=ii::gui::WidgetInputKind::key_up,.key=ii::gui::WidgetKey::enter}));
    require(scope->backend_0.events.empty()&&scope->backend_0.saves==1,"Reactive Enter opt-out did not suppress the button");
    scope->backend_0.enter_enabled.set(true);
    good(state->input({.kind=ii::gui::WidgetInputKind::key_down,.key=ii::gui::WidgetKey::enter}));
    good(state->input({.kind=ii::gui::WidgetInputKind::key_up,.key=ii::gui::WidgetKey::enter}));
    require(scope->backend_0.saves==2,"Keyboard activation did not invoke the action once");
    require(scope->backend_0.events==std::vector<std::string>{"pressed","released","method","after","last","clicked"},
            "Generated button did not deliver keyboard events in order");
    scope->backend_0.events.clear();
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=x,.y=y}));
    good(state->input({.kind=ii::gui::WidgetInputKind::focus_lost}));
    require(scope->backend_0.events==std::vector<std::string>{"pressed","canceled"},
            "Generated button did not deliver cancellation");
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
    auto plain=find(state,"/plain"),nested=find(state,"/nested");
    auto& model=scope->backend_0;model.events.clear();
    model.methods={[&](const auto&){model.events.emplace_back("first");},[&](const auto&){model.events.emplace_back("second");}};
    model.nested_methods={{{[&](const auto&){model.events.emplace_back("nested1");}}},
                          {{[&](const auto&){model.events.emplace_back("nested2");}}}};
    scope->publish_native_changes();good(plain.activate());good(nested.activate());
    require(model.events==std::vector<std::string>{"first","second","nested1","nested2"},
            "Reactive or nested generated methods were not delivered in order");
    model.events.clear();good(find(state,"/invoker").activate());good(find(state,"/trigger").activate());
    require(model.events==std::vector<std::string>{"object invoke","object trigger"},
            "Generated methods did not invoke object adapters");
    model.events.clear();model.methods.assign(257,[](const auto&){});bool rejected=false;
    try{scope->publish_native_changes();}catch(const std::exception&){rejected=true;}
    require(rejected&&!take(graph->failures()).empty(),"Excess button methods were not rejected");
    good(plain.activate());require(model.events==std::vector<std::string>{"first","second"},
        "An invalid method list replaced the live list");
    model.events.clear();model.methods={
        [&](const auto&){model.events.emplace_back("old1");model.methods={[&](const auto&){model.events.emplace_back("new");}};scope->publish_native_changes();},
        [&](const auto&){model.events.emplace_back("old2");}};
    scope->publish_native_changes();good(plain.activate());good(plain.activate());
    require(model.events==std::vector<std::string>{"old1","old2","new"},"Reentrant method replacement changed the active dispatch");
    auto selected=find(state,"/secondary");
    scope->backend_0.selected.set(true);
    const auto selected_snapshot=take(selected.snapshot());
    const auto selected_fill=background(take(state->drawing_snapshot({640,480})),selected_snapshot.bounds);
    const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    require(selected_snapshot.checked&&near(selected_fill.red,linear(.2))&&
            near(selected_fill.green,linear(.4))&&near(selected_fill.blue,linear(.6)),
            "Reactive selectedColor was not drawn on the selected button");
    const auto segment=find(state,"/segment");
    const auto segment_bounds=take(segment.snapshot()).bounds;
    auto segment_frame=take(state->drawing_snapshot({640,480}));
    auto segment_fill=background(segment_frame,segment_bounds);
    require(take(segment.snapshot()).checked&&near(segment_fill.red,linear(lvui::figma_defaults::panel_surface_solid.red))&&
            has_title_color(segment_frame,"Segment"),
            "Generated selected segment did not use the default LVUI button appearance");
    scope->backend_0.selection_style.set(lvui::ButtonSelectionStyle::accent);
    segment_fill=background(take(state->drawing_snapshot({640,480})),segment_bounds);
    require(near(segment_fill.red,linear(lvui::figma_defaults::accent_blue_selected.red)),
            "Reactive button selection style did not restore the accent appearance");
    scope->backend_0.selection_style.set(lvui::ButtonSelectionStyle::segmented);
    segment_frame=take(state->drawing_snapshot({640,480}));
    require(has_title_color(segment_frame,"Segment"),"Segment selection style did not recover reactively");
    good(selected.activate());
    require(take(selected.snapshot()).checked,"A controlled selection toggled itself on activation");
    scope->backend_0.selected_color.set({.6,.4,.2,1});
    const auto recolored=background(take(state->drawing_snapshot({640,480})),selected_snapshot.bounds);
    require(near(recolored.red,linear(.6))&&near(recolored.blue,linear(.2)),
            "Changing selectedColor did not repaint the selected button");
    model.events.clear();model.methods={[&](const auto&){model.events.emplace_back("retire");good(host.clear_content());},
                                        [&](const auto&){model.events.emplace_back("unexpected");}};
    scope->publish_native_changes();good(plain.activate());
    require(model.events==std::vector<std::string>{"retire"},"Retirement did not stop later button methods");
    std::weak_ptr<View::Scope> weak=scope;
    scope.reset();graph.reset();
    require(weak.expired()&&!button.snapshot()&&!icon.snapshot(),"Retired button retained its scope");
    state->close();
}
