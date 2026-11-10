#include "native_card_composition.h"
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class T> T take(ii::core::Result<T> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
    return std::move(*value);
}
void good(ii::core::Result<void> value) {
    if (!value) { if (value.error().cause) std::rethrow_exception(value.error().cause); throw std::runtime_error(value.error().message); }
}
bool contains(const ii::gui::CanvasFrame& frame,std::string_view text) {
    for (const auto& run : frame.texts) if (run.text == text) return true;
    for (const auto& layer : frame.overlays) for (const auto& run : layer.texts) if (run.text == text) return true;
    return false;
}
bool image_present(const ii::gui::CanvasFrame& frame) {
    if (!frame.images.empty()) return true;
    for (const auto& layer : frame.overlays) if (!layer.images.empty()) return true;
    return false;
}
}
int main() {
    using namespace lvui; using namespace ii::gui;using lvui::CardType;using lvui::CardDisplayState;
    auto tree = std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task) { task(); });
    auto host = take(tree->content_host({},"host",{340,340}));
    native::CardInput input; auto& c = input.configuration; c.type = CardType::project;
    c.title = "Project"; c.description = "Shared project"; c.status_text = "Ready";
    c.rows = {{"Owner","User"}}; c.progress = .5; c.progress_text = "50%";
    input.icon = take(rgba_image(1,1,{255,0,0,255}));
    std::optional<native::NativeCard> card; unsigned clicks=0, menus=0, actions=0; std::vector<bool> selection;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void> {
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});
        if (!column) return std::unexpected(column.error());
        auto value=native::NativeCard::create(builder,*column,"card",input,
            {.clicked=[&]{++clicks;},.selected_changed=[&](bool value){selection.push_back(value);},
             .menu_requested=[&]{++menus;},.action_triggered=[&]{++actions;}});
        if (!value) return std::unexpected(value.error()); card=*value; return {};
    }));
    require(take(card->root().snapshot()).size.width == 256 && take(card->root().snapshot()).size.height >= 280 &&
            take(card->surface().action_options()).accessibility_name == "Project", "Card geometry or accessible title changed");
    auto frame=take(tree->drawing_snapshot({340,340}));
    require(contains(frame,"Project") && contains(frame,"Shared project") && contains(frame,"Owner") &&
            contains(frame,"50%") && image_present(frame), "Card omitted its owned text, progress, or image");
    const auto check_outer=[&](bool has_fill,float radius,float border_width){
        auto shot=take(tree->drawing_snapshot({340,340}));const auto bounds=take(card->surface().snapshot()).bounds;
        bool fill=false,border=false;
        const auto check=[&](const auto& panels){for(const auto& panel:panels)if(panel.bounds==bounds){
            if(panel.stroke_width==0)fill=true;
            if(panel.stroke_width==border_width && panel.radius==radius)border=true;
        }};
        check(shot.panels);for(const auto& layer:shot.overlays)check(layer.panels);
        require(fill==has_fill && border,"Card surface fill, radius or border state differs from the contract");
    };
    for(int kind=2;kind<8;++kind)for(int state=1;state<=3;++state){
        auto current=take(card->options());current.configuration.type=static_cast<CardType>(kind);
        current.configuration.display_state=static_cast<CardDisplayState>(state);good(card->set_options(current));
        check_outer(false,0,state==3?2.f:1.f);
    }
    auto original=take(card->options());original.configuration.type=CardType::file;
    original.configuration.display_state=CardDisplayState::normal;good(card->set_options(original));check_outer(true,12,1);
    original.configuration.type=CardType::project;original.configuration.corner_radius=5;good(card->set_options(original));check_outer(false,5,1);
    original.configuration.corner_radius.reset();original.configuration.display_state=CardDisplayState::automatic;good(card->set_options(original));
    good(card->surface().activate());
    require(clicks==1 && selection==std::vector<bool>{true} && take(card->options()).configuration.selected,
            "Card selection and click order failed");
    good(card->menu().activate());good(card->action().activate());
    require(menus==1 && actions==1 && clicks==1, "Card subactions triggered the wrong callback");
    auto changed=take(card->options()); changed.configuration.title="Renamed"; changed.configuration.width=300;
    good(card->set_options(changed));
    require(take(card->root().snapshot()).size.width==300 && take(card->surface().action_options()).accessibility_name=="Renamed",
            "Card did not update retained content");
    auto bad=changed;bad.configuration.progress=2;
    require(!card->set_options(bad) && take(card->options())==changed, "Invalid card state replaced a good card");
    bad=changed;bad.icon.reset();bad.configuration.icon_name="missing-icon";
    require(!card->set_options(bad) && take(card->options())==changed, "Invalid icon replaced a good card");
    changed.configuration.enabled=false;good(card->set_options(changed));
    good(card->surface().activate());good(card->menu().activate());good(card->action().activate());
    require(clicks==1 && menus==1 && actions==1, "Disabled card delivered an event");
    bool wrong=false;std::thread other([&]{auto value=card->set_options(input);wrong=!value && value.error().code==ii::core::ErrorCode::wrong_executor;});
    other.join();require(wrong,"Card accepted a foreign-thread update");
    good(host.clear_content());require(!card->set_options(input),"Retired card remained writable");

    native::CardInput preview;preview.configuration.type=CardType::file_preview;preview.configuration.preview_title="Preview";
    preview.configuration.show_menu=true;preview.configuration.show_action=true;
    preview.preview=take(rgba_image(2,1,{255,0,0,255,0,255,0,255}));
    unsigned preview_clicks=0,preview_changes=0,preview_actions=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void> {
        auto value=native::NativeCard::create(builder,{},"preview",preview,
            {.clicked=[&]{++preview_clicks;},.selected_changed=[&](bool){++preview_changes;},
             .menu_requested=[&]{++preview_actions;},.action_triggered=[&]{++preview_actions;}});
        if(!value)return std::unexpected(value.error());card=*value;return {};
    }));
    auto preview_frame=take(tree->drawing_snapshot({340,340}));
    require(image_present(preview_frame)&&contains(preview_frame,"Preview"),"File preview omitted its image or title");
    good(card->surface().activate());good(card->menu().activate());good(card->action().activate());
    require(preview_clicks==1&&preview_changes==0&&preview_actions==2,"File preview actions or selection policy failed");
    good(host.clear_content());

    unsigned stale_clicks=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void> {
        auto value=native::NativeCard::create(builder,{},"removing",input,
            {.clicked=[&]{++stale_clicks;},.selected_changed=[&](bool){good(host.clear_content());}});
        if (!value) return std::unexpected(value.error());card=*value;return {};
    }));
    good(card->surface().activate());require(stale_clicks==0 && !card->root().snapshot(),"Retired card received a stale click");
    tree->close();
}
