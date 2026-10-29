#include "NativeWindowAccent.generated.h"
#include "NativeWindowAccentStatic.generated.h"
#include <LVUI/NativeWindowValues.h>
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
    require(near(static_appearance.primary.red,.5275065302848816)&&near(static_appearance.primary.green,.291259765625),
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
    const auto first_id=take(first.snapshot()).id,nested_id=take(nested.snapshot()).id;
    const auto window_background=take(state->appearance()).window;
    const auto blue=button_color(state,first),nested_blue=button_color(state,nested),badge_blue=button_color(state,badge);
    const auto custom_initial=button_color(state,custom);
    require(take(custom.action_options()).width==140,"Button did not apply its declared width");
    auto graph=scope->native_lifetime.lock();
    require(bool(graph),"Window binding graph was not retained");
    scope->backend_0.accent.set("purple");
    const auto purple=button_color(state,first),nested_purple=button_color(state,nested),badge_purple=button_color(state,badge);
    require(near(take(state->appearance()).primary.red,.5275065302848816)&&take(state->appearance()).window==window_background&&
            purple!=blue&&nested_purple!=nested_blue&&badge_purple!=badge_blue,
            "Reactive Window accent did not update the host and scoped buttons");
    auto static_state=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();},Static::appearance());
    good(static_state->construct([](ii::gui::WidgetBuilder& builder){return Static::build(builder);}));
    auto static_button=find(static_state,"primary");
    require(button_color(static_state,static_button)==purple,
            "Static and reactive Window accents produced different button colors");
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
    require(!take(graph->failures()).empty()&&button_color(state,first)==purple&&
            near(take(state->appearance()).primary.red,.5275065302848816),
            "An invalid accent replaced the last valid appearance");
    scope->backend_0.accent.set("green");
    const auto green=button_color(state,first),nested_green=button_color(state,nested),badge_green=button_color(state,badge);
    require(take(graph->failures()).empty()&&green!=purple,"Window accent did not recover after invalid input");

    reject=true;
    good(graph->begin_batch());
    scope->backend_0.width.set(700);scope->backend_0.accent.set("red");
    require(!graph->end_batch(),"Rejected Window geometry was not reported");
    require(!take(graph->failures()).empty()&&button_color(state,first)==green&&button_color(state,nested)==nested_green&&
            button_color(state,badge)==badge_green,
            "Rejected Window properties partially applied a new button palette");
    reject=false;scope->backend_0.width.set(720);
    require(take(graph->failures()).empty()&&button_color(state,first)!=green&&geometry.size.width==720,
            "Window and button accents did not recover after native rejection");
    good(first.activate());good(nested.activate());good(custom.activate());good(badge.activate());
    require(scope->backend_0.clicks==4,"Accent update disconnected button callbacks");
    std::weak_ptr<View::Scope> weak=scope;
    state->close();scope.reset();graph.reset();
    require(weak.expired()&&!first.snapshot(),"Window accent retained its retired scope");
}
