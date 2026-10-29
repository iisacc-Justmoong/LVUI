#include "NativeOwnedCheckBox.generated.h"
#include <iiGUI/Image.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
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
struct Ink final {ii::gui::Color fill;std::shared_ptr<const ii::gui::RasterImage> mark;std::optional<ii::gui::Color> tint;};
Ink ink(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    Ink result;bool found=false;
    const auto scan=[&](const auto& panels,const auto& images){
        for(const auto& panel:panels)if(near(panel.bounds.x,bounds.x+.5)&&near(panel.bounds.y,bounds.y+.5)&&
            near(panel.bounds.width,17)&&near(panel.bounds.height,17)&&near(panel.stroke_width,0)){
            result.fill=panel.top;found=true;
        }
        for(const auto& image:images)if(near(image.bounds.x,bounds.x+.5)&&near(image.bounds.y,bounds.y+.5)&&
            near(image.bounds.width,17)&&near(image.bounds.height,17)){result.mark=image.image;result.tint=image.tint;}
    };
    scan(frame.panels,frame.images);for(const auto& layer:frame.overlays)scan(layer.panels,layer.images);
    if(!found)throw std::runtime_error("Checkbox indicator was not painted");return result;
}
void interaction(){
    using View=lvui_generated::NativeOwnedCheckBox;
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(state->content_host({},"host",{640,480}));std::shared_ptr<View::Scope> scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets){return View::build(widgets,{}, {},&scope);}));
    auto check=find(state,"/check");auto initial=take(check.snapshot());
    require(initial.kind==ii::gui::WidgetKind::action_surface&&initial.action_role==ii::gui::ActionRole::check_box&&
            initial.accessibility_name=="Remember choice"&&!initial.checked&&near(initial.bounds.height,18),
            "Checkbox host semantics or initial state is wrong");
    require(!ink(take(state->drawing_snapshot({640,480})),initial.bounds).mark,"Unchecked checkbox painted a mark");
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
    scope->backend_0.color.set({2,0,0,1});
    require(!take(graph->failures()).empty()&&ink(take(state->drawing_snapshot({640,480})),take(check.snapshot()).bounds).fill==visual.fill,
            "Invalid color changed the retained checkbox");
    scope->backend_0.color.set({.5,.25,.75,.75});require(take(graph->failures()).empty(),"Checkbox did not recover from invalid color");
    scope->backend_0.text.set("Grid");require(take(check.snapshot()).checked,"An unrelated update reset user selection");
    scope->backend_0.checked.set(true);scope->backend_0.checked.set(false);
    require(!take(check.snapshot()).checked&&scope->backend_0.events.size()==1,"Programmatic update emitted a user callback");
    good(check.focus());good(state->input({.kind=ii::gui::WidgetInputKind::key_down,.key=ii::gui::WidgetKey::space}));
    good(state->input({.kind=ii::gui::WidgetInputKind::key_up,.key=ii::gui::WidgetKey::space}));
    require(take(check.snapshot()).checked&&scope->backend_0.events==std::vector<bool>({true,true}),"Keyboard activation failed");
    scope->backend_0.enabled.set(false);good(check.activate());
    require(!take(check.snapshot()).enabled&&scope->backend_0.events.size()==2,"Disabled checkbox admitted activation");
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
    std::optional<lvui::native::NativeCheckBox> component;int callbacks=0;
    good(host.set_content([&](ii::gui::WidgetBuilder& widgets)->ii::core::Result<void>{
        auto item=lvui::native::NativeCheckBox::create(widgets,{},"check",{},[&](bool){++callbacks;good(host.clear_content());});
        if(!item)return std::unexpected(item.error());check=item->root();component=*item;return {};
    }));
    bool rejected=false;std::thread worker([&]{lvui::CheckBoxConfiguration value;value.checked=true;const auto result=component->set_options(value);
        rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected&&!take(check.snapshot()).checked,"Foreign thread changed checkbox state");
    good(check.activate());require(callbacks==1&&!check.snapshot(),"Callback could not retire its generation safely");
    auto closed=component->set_options(lvui::CheckBoxConfiguration{});
    require(!closed&&closed.error().code==ii::core::ErrorCode::closed,"Retired checkbox remained writable");
    component.reset();state->close();
}
}
int main(){interaction();retirement();}
