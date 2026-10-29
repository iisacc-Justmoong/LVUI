#include <LVUI/NativeStepper.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
void retire_during_click(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"retiring",{100,60}));std::optional<lvui::native::NativeStepper> stepper;unsigned stepped=0;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto value=lvui::native::NativeStepper::create(builder,{},"stepper",{},
            {.stepped=[&](int){++stepped;},.clicked=[&]{good(host.clear_content());}});
        if(!value)return std::unexpected(value.error());stepper=*value;return {};
    }));
    auto id=take(stepper->root().snapshot()).id;
    good(tree->input({.kind=WidgetInputKind::increment,.target=id}));
    require(stepped==0&&!stepper->root().snapshot(),"Retired Stepper delivered a stale adjustment");tree->close();
}
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,120}));std::optional<lvui::native::NativeStepper> stepper;
    std::vector<int> steps;std::vector<std::string> events;lvui::StepperConfiguration options;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"root");if(!root)return std::unexpected(root.error());
        auto value=lvui::native::NativeStepper::create(builder,*root,"stepper",options,
            {.stepped=[&](int n){steps.push_back(n);},.clicked=[&]{events.push_back("click");},
             .pressed=[&]{events.push_back("press");},.released=[&]{events.push_back("release");},.canceled=[&]{events.push_back("cancel");}});
        if(!value)return std::unexpected(value.error());stepper=*value;return {};
    }));
    const auto frame=take(tree->drawing_snapshot({300,120}));
    auto snapshot=take(stepper->root().snapshot());auto bounds=snapshot.bounds;
    require(snapshot.accessibility_semantics&&snapshot.accessibility_semantics->role==AccessibilityRole::stepper&&
        snapshot.accessibility_semantics->can_increment&&snapshot.accessibility_semantics->can_decrement,
        "Owned Stepper did not publish adjustable semantics");
    bool glyph=false;for(const auto& run:frame.texts)glyph|=run.text=="↕";
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)glyph|=run.text=="↕";
    require(glyph,"Owned Stepper did not draw its direction indicator");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+8,.y=bounds.y+3}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+8,.y=bounds.y+3}));
    if(steps!=std::vector<int>{1}||events!=std::vector<std::string>{"press","release","click"}){
        std::string observed="Stepper pointer activation: bounds="+std::to_string(bounds.x)+","+std::to_string(bounds.y)+","+
            std::to_string(bounds.width)+","+std::to_string(bounds.height)+" steps=";
        for(auto step:steps)observed+=std::to_string(step)+",";
        observed+=" events=";for(const auto& event:events)observed+=event+",";
        throw std::runtime_error(observed);
    }
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+8,.y=bounds.y+15}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+8,.y=bounds.y+15}));
    require(steps.back()==-1,"Stepper bottom half did not decrement");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::up}));
    good(tree->input({.kind=WidgetInputKind::increment,.target=snapshot.id}));
    require(steps[steps.size()-1]==1&&steps[steps.size()-2]==1,"Keyboard or accessibility adjustment was lost");
    options.arrow=lvui::StepperArrow::down;good(stepper->set_options(options));snapshot=take(stepper->root().snapshot());
    require(!snapshot.accessibility_semantics->can_increment&&snapshot.accessibility_semantics->can_decrement,
        "Single-arrow accessibility actions were not constrained");
    const auto count=steps.size();good(tree->input({.kind=WidgetInputKind::increment,.target=snapshot.id}));
    good(stepper->root().step(1));good(stepper->root().step(-1));
    require(steps.size()==count+1&&steps.back()==-1,"Stepper accepted a prohibited direction or lost its public step action");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+8,.y=bounds.y+5}));
    good(tree->input({.kind=WidgetInputKind::focus_lost}));
    require(events.back()=="cancel","Stepper did not cancel an interrupted press");
    options.enabled=false;good(stepper->set_options(options));const auto disabled=steps.size();
    good(tree->input({.kind=WidgetInputKind::decrement,.target=snapshot.id}));require(steps.size()==disabled,"Disabled Stepper accepted input");
    auto invalid=options;invalid.height=0;require(!stepper->set_options(invalid),"Invalid Stepper update was accepted");
    bool rejected=false;std::thread worker([&]{auto r=stepper->set_options(options);rejected=!r&&r.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread changed a Stepper");
    good(host.clear_content());require(!stepper->set_options(options),"Retired Stepper accepted a change");tree->close();retire_during_click();
}
