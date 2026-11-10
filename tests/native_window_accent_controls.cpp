#include "NativeWindowAccentControls.generated.h"
#include "NativeWindowAccentControlsStatic.generated.h"
#include <LVUI/StyleTokens.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
using ii::gui::detail::WidgetState;
void require(bool condition, const std::string& message) { if (!condition) throw std::runtime_error(message); }
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
ii::gui::Widget find(const std::shared_ptr<WidgetState>& state, std::string_view suffix) {
    const auto snapshots=take(state->snapshots());
    const auto item=std::ranges::find_if(snapshots,[&](const auto& value){
        return value.id==suffix||value.id.ends_with(std::string("/")+std::string(suffix));
    });
    require(item!=snapshots.end(),"Missing control: "+std::string(suffix));
    return take(state->find(item->id));
}
ii::gui::Widget menu_item(const std::shared_ptr<WidgetState>& state,std::string_view label) {
    const auto snapshots=take(state->snapshots());
    const auto item=std::ranges::find_if(snapshots,[&](const auto& value){
        return value.text==label&&value.action_role==ii::gui::ActionRole::menu_item;
    });
    require(item!=snapshots.end(),"Missing generated menu item: "+std::string(label));
    return take(state->find(item->id));
}
double linear(double value) { return value<=.04045 ? value/12.92 : std::pow((value+.055)/1.055,2.4); }
bool near(double a,double b) { return std::abs(a-b)<1e-5; }
bool has_color(const std::shared_ptr<WidgetState>& state,const ii::gui::Widget& widget,lvui::StyleColor expected) {
    const auto frame=take(state->drawing_snapshot({640,420}));
    const auto bounds=take(widget.snapshot()).bounds;
    const auto matches=[&](const auto& panels){
        return std::ranges::any_of(panels,[&](const auto& panel){
            const auto& b=panel.bounds;const auto& c=panel.top;
            return b.width>0&&b.height>0&&b.x>=bounds.x-.01&&b.y>=bounds.y-.01&&
                b.x+b.width<=bounds.x+bounds.width+.01&&b.y+b.height<=bounds.y+bounds.height+.01&&
                near(c.red,linear(expected.red))&&near(c.green,linear(expected.green))&&
                near(c.blue,linear(expected.blue))&&near(c.alpha,expected.alpha);
        });
    };
    if(matches(frame.panels))return true;
    return std::ranges::any_of(frame.overlays,[&](const auto& layer){return matches(layer.panels);});
}
void expect_color(const std::shared_ptr<WidgetState>& state,std::string_view id,lvui::StyleColor color) {
    require(has_color(state,find(state,id),color),"Unexpected color for "+std::string(id)+" (red="+std::to_string(color.red)+")");
}
void expect_link_color(const std::shared_ptr<WidgetState>& state,std::string_view id,lvui::StyleColor color) {
    const auto actual=take(find(state,id).action_options()).text_color;
    require(actual&&near(actual->red,color.red)&&near(actual->green,color.green)&&
            near(actual->blue,color.blue)&&near(actual->alpha,color.alpha),"Unexpected link color for "+std::string(id));
}
void expect_focus(const std::shared_ptr<WidgetState>& state,std::string_view id,lvui::StyleColor color) {
    good(find(state,id).focus());expect_color(state,id,color);
}
}

int main() {
    using View=lvui_generated::NativeWindowAccentControls;
    using Static=lvui_generated::NativeWindowAccentControlsStatic;
    using namespace ii::gui;
    WindowGeometryValue geometry{"Controls",{640,420},{320,220}};
    bool reject=false;
    detail::WidgetFrameHooks hooks;
    hooks.window_geometry=[&](const WindowGeometry& next)->ii::core::Result<void>{
        if(reject)return ii::core::fail(ii::core::ErrorCode::failed,"Native geometry rejected");
        auto updated=resolve_window_geometry(geometry,next);
        if(!updated)return std::unexpected(updated.error());
        geometry=*updated;return {};
    };
    auto state=std::make_shared<WidgetState>([](ii::runtime::Task task){task();},View::appearance(),std::move(hooks));
    std::shared_ptr<View::Scope> scope;
    good(state->construct([&](WidgetBuilder& builder){return View::build(builder,{}, {},&scope);}));
    good(state->commit_window_properties());
    auto graph=scope->native_lifetime.lock();require(bool(graph),"Missing native binding graph");
    constexpr auto blue=lvui::figma_defaults::accent_blue_default;
    constexpr auto purple=lvui::figma_defaults::accent_purple_default;
    constexpr lvui::StyleColor red{1,0,0,1};
    constexpr std::array controls{"radio","check","toggle","slider","nestedSlider","stepper","tab","dropdown","menuItem"};
    for(auto id:controls)expect_color(state,id,blue);
    for(auto id:{"customRadio","customCheck","customToggle","customSlider","customStepper","customDropdown"})expect_color(state,id,red);
    expect_link_color(state,"link",blue);expect_link_color(state,"customLink",red);
    expect_focus(state,"combo",blue);expect_focus(state,"link",blue);
    auto field=find(state,"field");good(field.focus());
    good(state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::select_all}));
    auto selection=take(field.snapshot());
    require(selection.text_selection==TextRange{0,6},"Input selection was not initialized");
    expect_color(state,"field",blue);
    auto blue_selection=blue;blue_selection.alpha*=.38;expect_color(state,"field",blue_selection);
    scope->backend_0.accent.set("purple");
    require(take(graph->failures()).empty(),"Accent update reported a failure");
    for(auto id:controls)expect_color(state,id,purple);
    expect_color(state,"field",purple);
    auto purple_selection=purple;purple_selection.alpha*=.38;expect_color(state,"field",purple_selection);
    require(take(field.snapshot()).text_selection==selection.text_selection,"Accent changed the text selection");
    for(auto id:{"customRadio","customCheck","customToggle","customSlider","customStepper","customDropdown"})expect_color(state,id,red);
    require(scope->backend_0.edits==0&&scope->backend_0.toggles==0&&scope->backend_0.moves==0&&
            scope->backend_0.steps==0&&scope->backend_0.tabs==0,"Accent change emitted an interaction callback");
    expect_link_color(state,"link",purple);expect_link_color(state,"customLink",red);
    expect_focus(state,"combo",purple);expect_focus(state,"link",purple);good(field.focus());

    auto static_state=std::make_shared<WidgetState>([](ii::runtime::Task task){task();},Static::appearance());
    good(static_state->construct([](WidgetBuilder& builder){return Static::build(builder);}));
    for(auto id:controls)expect_color(static_state,id,purple);
    expect_link_color(static_state,"link",purple);
    expect_focus(static_state,"combo",purple);expect_focus(static_state,"link",purple);
    good(find(static_state,"field").focus());expect_color(static_state,"field",purple);

    good(state->input({.kind=WidgetInputKind::composition,.text="한"}));
    const auto composition=take(field.snapshot());
    require(composition.marked_range.has_value(),"Input composition did not start");
    scope->backend_0.accent.set("green");
    const auto after_composition=take(field.snapshot());
    require(after_composition.marked_range==composition.marked_range&&after_composition.text_selection==composition.text_selection&&
            after_composition.composition_text==composition.composition_text&&scope->backend_0.edits==0,
            "Accent update interrupted input composition");
    good(state->input({.kind=WidgetInputKind::text,.text="한"}));
    require(scope->backend_0.edits==1,"Composition did not commit once");
    good(state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::undo}));
    require(take(field.text_content({0,6})).text=="Sample","Accent update lost input undo history");
    good(state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::redo}));
    require(take(field.text_content({0,1})).text=="한","Accent update lost input redo history");

    auto custom_field=find(state,"customField");good(custom_field.focus());
    good(state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::select_all}));
    expect_color(state,"customField",red);
    constexpr lvui::StyleColor custom_green{0,1,0,1};
    expect_color(state,"customField",custom_green);
    auto custom_slider=find(state,"customSlider");good(custom_slider.focus());
    // This root deliberately remains an unpublished, unattached window test.
    // Focus motion starts after host publication; its generated custom color is
    // verified separately in the published OwnedSlider.Generated fixture.
    expect_color(state,"customSlider",red);
    expect_focus(state,"customCombo",red);expect_link_color(state,"customLink",red);
    const auto before=take(find(state,"slider").snapshot()).id;
    reject=true;good(graph->begin_batch());scope->backend_0.width.set(700);scope->backend_0.accent.set("red");
    require(!graph->end_batch(),"Rejected window change was accepted");
    const auto green=lvui::figma_defaults::accent_green_default;
    for(auto id:controls)expect_color(state,id,green);
    expect_link_color(state,"link",green);expect_link_color(state,"customLink",red);
    reject=false;scope->backend_0.width.set(720);
    require(take(graph->failures()).empty()&&geometry.size.width==720,"Window palette did not recover");
    for(auto id:controls)expect_color(state,id,lvui::figma_defaults::accent_red_default);
    expect_link_color(state,"link",lvui::figma_defaults::accent_red_default);
    require(take(find(state,"slider").snapshot()).id==before,"Accent update replaced a retained control");

    auto check=find(state,"check"),toggle=find(state,"toggle"),slider=find(state,"slider");
    good(check.activate());
    require(!take(check.snapshot()).checked,"CheckBox did not respond to activation");
    good(slider.focus());good(state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    const auto moved=take(slider.snapshot()).value;
    require(!near(moved,.5),"Slider did not respond to keyboard input");
    scope->backend_0.accent.set("purple");
    auto dropdown=find(state,"dropdown");auto area=take(dropdown.snapshot()).bounds;
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=area.x+5,.y=area.y+5}));
    expect_color(state,"dropdown",lvui::figma_defaults::accent_purple_hover);
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=area.x+5,.y=area.y+5}));
    expect_color(state,"dropdown",lvui::figma_defaults::accent_purple_pressed);
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=area.x+5,.y=area.y+5}));
    auto custom_dropdown=find(state,"customDropdown");area=take(custom_dropdown.snapshot()).bounds;
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=area.x+5,.y=area.y+5}));
    expect_color(state,"customDropdown",red);
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=area.x+5,.y=area.y+5}));
    expect_color(state,"customDropdown",red);
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=area.x+5,.y=area.y+5}));
    auto track=take(toggle.snapshot()).bounds;
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=track.x+5,.y=track.y+5}));
    expect_color(state,"toggle",lvui::figma_defaults::accent_purple_hover);
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=track.x+5,.y=track.y+5}));
    expect_color(state,"toggle",lvui::figma_defaults::accent_purple_pressed);
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=track.x+5,.y=track.y+5}));
    require(!take(toggle.snapshot()).checked,"ToggleSwitch did not respond to activation");
    const auto toggles=scope->backend_0.toggles,moves=scope->backend_0.moves;
    scope->backend_0.accent.set("green");
    require(!take(check.snapshot()).checked&&!take(toggle.snapshot()).checked&&near(take(slider.snapshot()).value,moved)&&
            scope->backend_0.toggles==toggles&&scope->backend_0.moves==moves,
            "Accent update changed user input or emitted interaction callbacks");
    auto custom_toggle=find(state,"customToggle");track=take(custom_toggle.snapshot()).bounds;
    good(state->input({.kind=WidgetInputKind::pointer_move,.x=track.x+5,.y=track.y+5}));
    expect_color(state,"customToggle",red);
    good(state->input({.kind=WidgetInputKind::pointer_down,.x=track.x+5,.y=track.y+5}));
    expect_color(state,"customToggle",red);
    good(state->input({.kind=WidgetInputKind::pointer_up,.x=track.x+5,.y=track.y+5}));
    static_state->close();std::weak_ptr<View::Scope> weak=scope;
    state->close();scope.reset();graph.reset();
    require(weak.expired()&&!field.snapshot(),"Accent controls retained a retired window scope");

    // Embedded headless content has no native Window endpoint. Inject the
    // committed accent observable to exercise the retained popup separately.
    auto menu_state=std::make_shared<WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(menu_state->content_host({},"host",{640,420}));std::shared_ptr<View::Scope> menu_scope;
    good(host.set_content([&](WidgetBuilder& builder){return View::build(builder,{}, {},&menu_scope);}));
    auto popup=find(menu_state,"popup");good(popup.open_at(440,250));
    require(take(popup.snapshot()).opened&&menu_scope->backend_0.opens==1,"Menu did not open");
    expect_color(menu_state,"popupItem",blue);
    menu_scope->window_accent.set(lvui::WindowAccent::purple);
    require(take(popup.snapshot()).opened&&menu_scope->backend_0.opens==1&&menu_scope->backend_0.closes==0,
            "Accent change disturbed an open menu");
    expect_color(menu_state,"popupItem",purple);
    good(popup.close_popup());require(menu_scope->backend_0.closes==1,"Menu did not close once");
    auto simple=find(menu_state,"simplePopup");good(simple.open_at(440,250));
    require(has_color(menu_state,menu_item(menu_state,"Simple"),purple),"Static list item did not inherit purple");
    good(menu_item(menu_state,"Simple").focus());
    good(menu_state->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    auto second=menu_item(menu_state,"Second");require(take(second.snapshot()).focused,"Menu keyboard focus did not advance");
    const auto before_first=take(menu_item(menu_state,"Simple").snapshot());
    const auto before_second=take(second.snapshot());
    menu_scope->window_accent.set(lvui::WindowAccent::green);
    require(take(simple.snapshot()).opened&&menu_scope->backend_0.opens==2&&menu_scope->backend_0.closes==1,
            "Changing accent disturbed the open static list menu");
    const auto after_first=take(menu_item(menu_state,"Simple").snapshot());const auto after_second=take(second.snapshot());
    require(after_second.focused&&before_first.selected==after_first.selected&&before_second.selected==after_second.selected,
            "Changing accent disturbed static list menu focus or selection");
    require(has_color(menu_state,second,green),"The focused static list item did not inherit green");
    good(simple.close_popup());std::weak_ptr<View::Scope> menu_weak=menu_scope;
    good(host.clear_content());menu_scope.reset();
    require(menu_weak.expired()&&!popup.snapshot(),"Accent menu retained a retired content scope");menu_state->close();

    auto static_menu_state=std::make_shared<WidgetState>([](ii::runtime::Task task){task();});
    auto static_host=take(static_menu_state->content_host({},"host",{640,420}));
    good(static_host.set_content([](WidgetBuilder& builder){return Static::build(builder);}));
    auto static_simple=find(static_menu_state,"simplePopup");good(static_simple.open_at(440,250));
    require(has_color(static_menu_state,menu_item(static_menu_state,"Simple"),purple),
            "Static window accent was not applied to the list menu");
    good(static_simple.close_popup());good(static_host.clear_content());static_menu_state->close();
}
