#include "NativeWindowAccent.generated.h"
#include "NativeWindowAccentStatic.generated.h"
#include <LVUI/NativeWindowValues.h>
#include <LVUI/StyleTokens.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> result) {
    if (!result) {
        if (result.error().cause) std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
    return std::move(*result);
}
void good(ii::core::Result<void> result) {
    if (!result) {
        if (result.error().cause) std::rethrow_exception(result.error().cause);
        throw std::runtime_error(result.error().message);
    }
}
ii::gui::Widget find(const std::shared_ptr<ii::gui::detail::WidgetState>& state, std::string_view suffix) {
    const auto snapshots=take(state->snapshots());
    const auto found=std::ranges::find_if(snapshots,[&](const auto& item){return item.id==suffix||item.id.ends_with(std::string("/")+std::string(suffix));});
    if(found==snapshots.end())throw std::runtime_error("Window accent button did not mount");
    return take(state->find(found->id));
}
ii::gui::Color background(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds) {
    const auto search=[&](const auto& panels)->std::optional<ii::gui::Color>{
        for(const auto& panel:panels)if(panel.bounds==bounds&&panel.stroke_width==0)return panel.top;
        return std::nullopt;
    };
    if(auto result=search(frame.panels))return *result;
    for(const auto& layer:frame.overlays)if(auto result=search(layer.panels))return *result;
    throw std::runtime_error("Window accent button was not drawn");
}
ii::gui::Color button_color(const std::shared_ptr<ii::gui::detail::WidgetState>& state,const ii::gui::Widget& button) {
    const auto frame=take(state->drawing_snapshot({640,420}));
    return background(frame,take(button.snapshot()).bounds);
}
ii::gui::Color pressed_color(const std::shared_ptr<ii::gui::detail::WidgetState>& state,const ii::gui::Widget& button) {
    const auto bounds=take(button.snapshot()).bounds;
    const auto x=bounds.x+bounds.width/2,y=bounds.y+bounds.height/2;
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_move,.x=x,.y=y}));
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_down,.x=x,.y=y}));
    const auto color=button_color(state,button);
    good(state->input({.kind=ii::gui::WidgetInputKind::pointer_up,.x=x,.y=y}));
    return color;
}
ii::gui::Color linear(lvui::StyleColor value) {
    const auto c=[](double x){return float(x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4));};
    return {c(value.red),c(value.green),c(value.blue),float(value.alpha)};
}
ii::gui::Color material_color(const std::shared_ptr<ii::gui::detail::WidgetState>& state,const ii::gui::Widget& material) {
    const auto frame=take(state->drawing_snapshot({640,420}));
    const auto bounds=take(material.snapshot()).bounds;
    for(const auto& layer:frame.overlays)if(layer.backdrop&&layer.backdrop->bounds==bounds&&
            layer.backdrop->underlay&&!layer.backdrop->underlay_tint){
        const auto image=layer.backdrop->underlay;const auto extent=layer.backdrop->underlay_bounds;
        const auto x=std::min(image->width()-1,unsigned((bounds.x+bounds.width/2-extent.x)*image->width()/extent.width));
        const auto y=std::min(image->height()-1,unsigned((bounds.y+bounds.height/2-extent.y)*image->height()/extent.height));
        const auto at=(std::size_t(y)*image->width()+x)*4;const auto rgba=image->rgba();
        return {rgba[at]/255.f,rgba[at+1]/255.f,rgba[at+2]/255.f,rgba[at+3]/255.f};
    }
    throw std::runtime_error("Window accent material was not drawn");
}
ii::gui::Color neutral_window_material_color(const std::shared_ptr<ii::gui::detail::WidgetState>& state,const ii::gui::Widget& material) {
    const auto frame=take(state->drawing_snapshot({640,420}));const auto bounds=take(material.snapshot()).bounds;
    for(const auto& layer:frame.overlays)if(layer.backdrop&&layer.backdrop->bounds==bounds){
        require(!layer.backdrop->underlay,"Neutral window allocated an accent underlay");return layer.backdrop->tint;
    }
    throw std::runtime_error("Neutral window material was not drawn");
}
bool near(double a,double b){return std::abs(a-b)<1e-5;}
enum class OtherAccent { blue };
template<class T> concept AccentInput=requires(T value){lvui::native::window_accent(value);};
static_assert(AccentInput<lvui::WindowAccent> && AccentInput<std::string>);
static_assert(!AccentInput<int> && !AccentInput<OtherAccent>);
}

int main() {
    using View=lvui_generated::NativeWindowAccent;
    using Static=lvui_generated::NativeWindowAccentStatic;
    const auto static_appearance=Static::appearance();
    const auto purple_default=lvui::figma_defaults::accent_purple_default;
    require(near(static_appearance.primary.red,purple_default.red)&&near(static_appearance.primary.green,purple_default.green),
            "Static Window accent did not select its own palette");
    require(lvui::native::window_accent("green")==lvui::WindowAccent::green,
            "Window accent name did not resolve to the LVUI enum");
    require(lvui::native::window_accent<OtherAccent>("blue")==OtherAccent::blue,
            "Explicit native enum conversion lost its existing contract");
    for(std::size_t i=0;i<lvui::window_accent_names.size();++i)
        require(lvui::native::window_accent(lvui::window_accent_names[i])==static_cast<lvui::WindowAccent>(i)&&
                lvui::fallback_window_accent(static_cast<lvui::WindowAccent>(i)).has_value(),
                "Window accent names and fallback colors disagree");
    bool invalid=false;
    try{(void)lvui::native::window_accent(static_cast<lvui::WindowAccent>(100));}
    catch(const std::invalid_argument&){invalid=true;}
    require(invalid,"Invalid LVUI accent enum was accepted");

    ii::gui::WindowGeometryValue geometry{"Initial",{640,420},{320,220}};
    bool reject=false;
    ii::gui::detail::WidgetFrameHooks hooks;
    hooks.window_geometry=[&](const ii::gui::WindowGeometry& next)->ii::core::Result<void>{
        if(reject)return ii::core::fail(ii::core::ErrorCode::failed,"Native geometry rejected");
        auto updated=ii::gui::resolve_window_geometry(geometry,next);
        if(!updated)return std::unexpected(updated.error());
        geometry=*updated;return {};
    };
    auto state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},View::appearance(),std::move(hooks));
    ii::gui::WindowPropertiesTarget target;
    std::shared_ptr<View::Scope> scope;
    good(state->construct([&](ii::gui::WidgetBuilder& builder)->ii::core::Result<void>{
        target=take(builder.window_properties());
        return View::build(builder,{}, {},&scope);
    }));
    good(state->commit_window_properties());
    auto first=find(state,"primary"),nested=find(state,"nested"),custom=find(state,"custom"),badge=find(state,"badge");
    auto normal_icon=find(state,"normalIcon"),fixed_icon=find(state,"fixedIcon");
    require(pressed_color(state,normal_icon)==linear(lvui::figma_defaults::accent_blue_selected),
            "Generated default icon button did not use the blue selected token when pressed");
    require(pressed_color(state,fixed_icon)==linear({0,1,0,1}),
            "Explicit borderless pressed color was not preserved");
    auto material=find(state,"material"),nested_material=find(state,"nestedMaterial"),custom_material=find(state,"customMaterial");
    const auto first_id=take(first.snapshot()).id,nested_id=take(nested.snapshot()).id;
    const auto window_background=take(state->appearance()).window;
    const auto blue=button_color(state,first),nested_blue=button_color(state,nested),badge_blue=button_color(state,badge);
    const auto custom_initial=button_color(state,custom);
    const auto material_blue=material_color(state,material),nested_material_blue=material_color(state,nested_material);
    const auto custom_material_initial=neutral_window_material_color(state,custom_material);
    require(take(custom.action_options()).width==140,"Button did not apply its declared width");
    auto graph=scope->native_lifetime.lock();
    require(bool(graph),"Window binding graph was not retained");
    scope->backend_0.accent.set("purple");
    const auto purple=button_color(state,first),nested_purple=button_color(state,nested),badge_purple=button_color(state,badge);
    const auto material_purple=material_color(state,material),nested_material_purple=material_color(state,nested_material);
    require(near(take(state->appearance()).primary.red,purple_default.red),
            "Reactive Window accent did not update the host accent");
    require(take(state->appearance()).window==window_background,
            "Reactive Window accent changed the window background");
    if(purple==blue){
        const auto errors=take(graph->failures());
        throw std::runtime_error("Reactive Window accent did not update the primary button; color="+
            std::to_string(blue.red)+","+std::to_string(blue.green)+","+std::to_string(blue.blue)+
            "; binding errors="+std::to_string(errors.size()));
    }
    require(nested_purple!=nested_blue,"Reactive Window accent did not update the nested button");
    require(badge_purple!=badge_blue,"Reactive Window accent did not update the badge");
    require(material_blue!=material_purple&&nested_material_blue!=nested_material_purple&&
            neutral_window_material_color(state,custom_material)==custom_material_initial,
            "Material lost inherited, nested, or explicit Window accent");
    require(pressed_color(state,normal_icon)==linear(lvui::figma_defaults::accent_purple_selected)&&
            pressed_color(state,fixed_icon)==linear({0,1,0,1}),
            "Reactive accent failed to update the default press or overwrote an explicit color");
    auto static_state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},Static::appearance());
    good(static_state->construct([](ii::gui::WidgetBuilder& builder){return Static::build(builder);}));
    auto static_button=find(static_state,"primary");
    require(pressed_color(static_state,find(static_state,"normalIcon"))==
            linear(lvui::figma_defaults::accent_purple_selected),
            "Static accent did not select the icon button pressed token");
    require(button_color(static_state,static_button)==purple,
            "Static and reactive Window accents produced different button colors");
    require(material_color(static_state,find(static_state,"material"))==material_purple,
            "Static and reactive Window accents produced different material colors");
    static_state->close();
    require(button_color(state,custom)==custom_initial,"Window accent overwrote an explicit button color");
    require(take(first.snapshot()).id==first_id&&take(nested.snapshot()).id==nested_id,
            "Accent update replaced retained buttons");
    scope->backend_0.label.set("Renamed");
    require(take(nested.snapshot()).text=="Renamed"&&button_color(state,nested)==nested_purple,
            "Nested reactive inputs lost the inherited Window accent");
    scope->backend_0.customColor.set(ii::gui::WidgetColor{.8,.1,.2,1});
    const auto custom_changed=button_color(state,custom);
    require(custom_changed!=custom_initial,"Explicit reactive button color did not update");
    scope->backend_0.customWidth.set(0);
    scope->backend_0.customColor.set(ii::gui::WidgetColor{.1,.2,.8,1});
    require(!take(graph->failures()).empty()&&button_color(state,custom)==custom_changed&&
            take(custom.action_options()).width==140,"Invalid button styling replaced its last valid state");
    scope->backend_0.customWidth.set(160);
    require(take(graph->failures()).empty()&&button_color(state,custom)!=custom_changed&&
            take(custom.action_options()).width==160,"Button styling did not recover after invalid width");
    scope->backend_0.accent.set("invalid");
    require(!take(graph->failures()).empty()&&button_color(state,first)==purple&&material_color(state,material)==material_purple&&
            near(take(state->appearance()).primary.red,purple_default.red),
            "An invalid accent replaced the last valid appearance");
    scope->backend_0.accent.set("green");
    const auto green=button_color(state,first),nested_green=button_color(state,nested),badge_green=button_color(state,badge);
    const auto material_green=material_color(state,material),nested_material_green=material_color(state,nested_material);
    require(take(graph->failures()).empty()&&green!=purple&&material_green!=material_purple&&
            nested_material_green!=nested_material_purple&&neutral_window_material_color(state,custom_material)==custom_material_initial,
            "Window accent did not recover after invalid input");

    reject=true;
    good(graph->begin_batch());
    scope->backend_0.width.set(700);scope->backend_0.accent.set("red");
    require(!graph->end_batch(),"Rejected Window geometry was not reported");
    require(!take(graph->failures()).empty()&&button_color(state,first)==green&&button_color(state,nested)==nested_green&&
            button_color(state,badge)==badge_green&&material_color(state,material)==material_green&&
            material_color(state,nested_material)==nested_material_green,
            "Rejected Window properties partially applied a new palette");
    require(scope->window_accent.get()==lvui::WindowAccent::green,
            "Rejected Window properties partially changed the committed accent");
    reject=false;scope->backend_0.width.set(720);
    require(take(graph->failures()).empty()&&button_color(state,first)!=green&&material_color(state,material)!=material_green&&geometry.size.width==720,
            "Window and button accents did not recover after native rejection");
    good(first.activate());good(nested.activate());good(custom.activate());good(badge.activate());
    require(scope->backend_0.clicks==4,"Accent update disconnected button callbacks");
    std::weak_ptr<View::Scope> weak=scope;
    state->close();scope.reset();graph.reset();
    require(weak.expired()&&!first.snapshot(),"Window accent retained its retired scope");

    // A content host publishes popup state without opening an operating-system window.
    auto popup_state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(popup_state->content_host({},"host",{640,420}));std::shared_ptr<View::Scope> popup_scope;
    good(host.set_content([&](ii::gui::WidgetBuilder& builder){return View::build(builder,{}, {},&popup_scope);}));
    auto popup_graph=popup_scope->native_lifetime.lock();require(bool(popup_graph),"Popover accent graph was not retained");
    auto popover=find(popup_state,"accentPopover"),nested_popover=find(popup_state,"nestedPopover");
    auto fixed_popover=find(popup_state,"fixedPopover"),reactive_popover=find(popup_state,"reactivePopover");
    const auto popover_color=[&](const auto& tree,const auto& control){
        good(control.open_at(450,300));const auto result=material_color(tree,control);good(control.close_popup());return result;
    };
    const auto blue_popup=popover_color(popup_state,popover);
    const auto fixed_red_popup=popover_color(popup_state,fixed_popover);
    require(popover_color(popup_state,nested_popover)==blue_popup&&fixed_red_popup!=blue_popup,
            "Popover did not inherit its default accent or retain an explicit primary color");
    good(popover.open_at(450,300));const auto open_popover_id=take(popover.snapshot()).id;
    popup_scope->window_accent.set(lvui::WindowAccent::purple);
    const auto purple_popup=material_color(popup_state,popover);
    require(take(popup_graph->failures()).empty()&&take(popover.snapshot()).opened&&
            take(popover.snapshot()).id==open_popover_id&&purple_popup!=blue_popup,
            "Reactive Window accent replaced or failed to recolor an open Popover");
    good(popover.close_popup());
    require(popover_color(popup_state,nested_popover)==purple_popup&&
            popover_color(popup_state,fixed_popover)==fixed_red_popup,
            "Window accent did not reach nested Popovers or overwrote an explicit color");
    good(reactive_popover.open_at(450,300));
    const auto custom_popup_initial=material_color(popup_state,reactive_popover);
    popup_scope->backend_0.customColor.set(ii::gui::WidgetColor{.8,.1,.2,1});
    const auto custom_popup_changed=material_color(popup_state,reactive_popover);
    require(custom_popup_changed!=custom_popup_initial,
            "Reactive explicit Popover color did not update");
    popup_scope->backend_0.customColor.set(ii::gui::WidgetColor{2,0,0,1});
    require(!take(popup_graph->failures()).empty()&&
            material_color(popup_state,reactive_popover)==custom_popup_changed,
            "Invalid reactive Popover color replaced its last valid material");
    popup_scope->backend_0.customColor.set(ii::gui::WidgetColor{.2,.4,.6,1});
    const auto custom_popup_recovered=material_color(popup_state,reactive_popover);
    require(take(popup_graph->failures()).empty()&&custom_popup_recovered!=custom_popup_changed,
            "Reactive Popover color did not recover after invalid input");
    popup_scope->window_accent.set(lvui::WindowAccent::green);
    require(material_color(popup_state,reactive_popover)==custom_popup_recovered,
            "Window accent overwrote a reactive explicit Popover color");
    good(reactive_popover.close_popup());
    require(popover_color(popup_state,popover)!=purple_popup,
            "Popover did not use the updated Window accent");
    std::weak_ptr<View::Scope> popup_weak=popup_scope;good(host.clear_content());popup_scope.reset();popup_graph.reset();
    require(popup_weak.expired()&&!popover.snapshot(),"Popover accent retained a retired scope");popup_state->close();

    auto static_popup_state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto static_host=take(static_popup_state->content_host({},"host",{640,420}));
    good(static_host.set_content([](ii::gui::WidgetBuilder& builder){return Static::build(builder);}));
    require(popover_color(static_popup_state,find(static_popup_state,"accentPopover"))==purple_popup,
            "Static and reactive Window accents produced different Popover colors");
    good(static_host.clear_content());static_popup_state->close();
}
