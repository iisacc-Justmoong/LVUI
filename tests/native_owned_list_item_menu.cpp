#include <LVUI/NativeListItemComposite.h>
#include "list_item_composite_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <any>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
struct Command final:lvui::ListItemMenuCommand {
    std::function<void(const lvui::ListItemMenuSelection&)> action;
    void invoke(const lvui::ListItemMenuSelection& value)const override{if(action)action(value);}
};
struct ReentrantIcons final:lvui::IconSource {
    mutable std::function<void()> on_resolve;
    lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view)const override{
        auto callback=std::move(on_resolve);on_resolve={};if(callback)callback();
        return std::make_shared<const lvui::IconAsset>(lvui::IconAsset{"<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 8 8'><rect width='8' height='8'/></svg>"});
    }
    std::uint64_t revision()const noexcept override{return 0;}
};
bool contains(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& run:frame.texts)if(run.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return true;
    return false;
}
std::size_t images(const ii::gui::CanvasFrame& frame){
    auto count=frame.images.size();for(const auto& layer:frame.overlays)count+=layer.images.size();return count;
}
bool has_color(const ii::gui::CanvasFrame& frame,const ii::gui::WidgetSnapshot& widget,lvui::StyleColor color){
    const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    const auto matches=[&](const auto& panels){for(const auto& panel:panels){const auto& b=panel.bounds;const auto& w=widget.bounds;const auto& c=panel.top;
        if(b.width>0&&b.height>0&&b.x>=w.x-.01&&b.y>=w.y-.01&&b.x+b.width<=w.x+w.width+.01&&b.y+b.height<=w.y+w.height+.01&&
           std::abs(c.red-linear(color.red))<1e-5&&std::abs(c.green-linear(color.green))<1e-5&&
           std::abs(c.blue-linear(color.blue))<1e-5&&std::abs(c.alpha-color.alpha)<1e-5)return true;}return false;};
    if(matches(frame.panels))return true;for(const auto& layer:frame.overlays)if(matches(layer.panels))return true;return false;
}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    auto data=std::make_shared<const std::any>(std::string("asset-42"));
    auto command=std::make_shared<Command>();unsigned commands=0;
    command->action=[&](const ListItemMenuSelection& selected){++commands;
        require(selected.entry&&selected.entry->payload==data&&std::any_cast<std::string>(*selected.entry->payload)=="asset-42",
                "Command did not receive the authored data");};
    ListItemMenuEntry keep;keep.label="Inspect";keep.shortcut="Ctrl+I";keep.icon_name="generaladd";keep.close_on_trigger=false;keep.state=MenuEntryState::selected;
    ListItemMenuEntry divider;divider.divider=true;
    ListItemMenuEntry disabled;disabled.label="Unavailable";disabled.enabled=false;
    ListItemMenuEntry leaf;leaf.label="Export PNG";leaf.payload=data;leaf.command=command;
    ListItemMenuEntry branch;branch.label="Export";branch.children={leaf};branch.show_chevron=false;branch.expanded=true;
    ListItemMenuEntry hidden_icon;hidden_icon.label="Plain";hidden_icon.icon_name="missing-hidden-menu-icon";hidden_icon.show_icon_slot=false;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::action_group;
    input.row.configuration.label="Project";input.menu_entries=std::vector<ListItemMenuEntry>{keep,divider,disabled,branch,hidden_icon};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{600,400}));std::optional<native::NativeListItemComposite> item;
    std::vector<ListItemMenuSelection> selections;std::vector<std::string> legacy,events;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"item",input,{
            .menu_item_triggered=[&](int index,const ListItemOption& option){legacy.push_back(std::to_string(index)+":"+option.text);},
            .action_event=[&](std::string name,const ListItemActionPayload&){events.push_back(name);},
            .menu_selection_triggered=[&](const ListItemMenuSelection& value){selections.push_back(value);}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    good(take(item->action(2)).activate());auto menu=take(item->action_menu());
    require(take(menu.snapshot()).opened,"Rich menu did not open");
    auto inspect=take(item->menu_item(0)),group=take(item->menu_item(3));
    require(take(group.action_options()).padding.left>20&&take(take(item->menu_item(4)).action_options()).padding.left<10&&
            take(group.action_options()).padding.right<10,
            "Rich menu icon slots or hidden chevrons did not follow their configuration");
    require(take(inspect.action_options()).accessibility_description=="Ctrl+I"&&
            contains(take(tree->drawing_snapshot({600,400})),"Ctrl+I")&&images(take(tree->drawing_snapshot({600,400})))>0,
            "Menu icon or shortcut was not rendered");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(group.snapshot()).focused&&take(inspect.action_options()).checked,
            "Keyboard navigation lost the authored selection or did not skip unavailable entries");
    auto submenu=take(item->menu_submenu({3}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(submenu.snapshot()).opened&&selections.empty(),"Keyboard navigation did not open the submenu without triggering it");
    const auto focused=take(group.snapshot()).focused;
    auto recolored=take(item->options());recolored.accent=WindowAccentPalette{.primary=figma_defaults::accent_purple_default,
        .subtle=figma_defaults::accent_purple_subtle,.selected=figma_defaults::accent_purple_selected};
    good(item->set_options(recolored));
    require(take(menu.snapshot()).opened&&take(submenu.snapshot()).opened&&take(group.snapshot()).focused==focused&&
            take(inspect.snapshot()).id==take(take(item->menu_item(0)).snapshot()).id&&selections.empty()&&
            has_color(take(tree->drawing_snapshot({600,400})),take(inspect.snapshot()),figma_defaults::accent_purple_default),
            "Restyling a rich menu closed or replaced its open submenu");
    const auto group_bounds=take(group.snapshot()).bounds,submenu_bounds=take(submenu.snapshot()).bounds;
    require(std::abs(submenu_bounds.y-(group_bounds.y+group_bounds.height))<1,
            "Expanded automatic submenu did not open below its item");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::left}));
    require(!take(submenu.snapshot()).opened&&take(group.snapshot()).focused,"Keyboard navigation did not return to the owning entry");
    good(inspect.activate());require(take(menu.snapshot()).opened&&selections.size()==1&&
            selections.back().path==std::vector<std::size_t>{0},"Keep-open entry closed the menu or lost its path");
    good(take(item->menu_item(2)).activate());require(selections.size()==1,"Disabled rich entry was triggered");
    good(group.activate());
    require(take(menu.snapshot()).opened&&take(submenu.snapshot()).opened&&selections.back().path==std::vector<std::size_t>{3},
            "Nested menu did not open from its owning entry");
    auto nested=take(item->menu_item({3,0}));good(nested.activate());
    require(!take(menu.snapshot()).opened&&!take(submenu.snapshot()).opened&&commands==1&&
            selections.back().path==std::vector<std::size_t>({3,0})&&selections.back().entry&&
            selections.back().entry->payload==data&&legacy.back()=="0:Export PNG"&&events.back()=="menuItem",
            "Nested action lost its path, payload, legacy event, or menu-close behavior");
    good(take(item->action(2)).activate());good(take(item->menu_item(3)).activate());auto stale=take(item->menu_item({3,0}));
    auto replacement=take(item->options());replacement.menu_entries=std::vector<ListItemMenuEntry>{keep};good(item->set_options(replacement));
    require(!take(menu.snapshot()).opened,"Replacing a rich menu left it open");
    const auto delivered=selections.size();(void)stale.activate();require(selections.size()==delivered,"A stale submenu delivered an event");
    good(take(item->action(2)).activate());require(!stale.snapshot(),"A replaced submenu generation remained mounted");
    auto invalid=replacement;(*invalid.menu_entries)[0].icon_name=std::string(257,'x');
    require(!item->set_options(invalid)&&take(item->options())==replacement,"An invalid rich menu replaced committed state");
    auto old_icon=take(item->menu_item(0));auto revised=replacement;++revised.row.icon_revision;good(item->set_options(revised));
    require(!take(menu.snapshot()).opened,"Changing the icon revision did not close the old menu");
    good(take(item->action(2)).activate());require(!old_icon.snapshot(),"Changing the icon revision retained an obsolete menu generation");
    auto preserved=take(item->menu_item(0));auto unknown_icon=revised;(*unknown_icon.menu_entries)[0].icon_name="missing-menu-icon";
    good(item->set_options(unknown_icon));good(take(item->action(2)).activate());
    require(take(item->failure()).has_value()&&preserved.snapshot()&&!take(menu.snapshot()).opened,
            "Failed icon resolution replaced the prior menu generation");
    good(item->set_options(revised));good(take(item->action(2)).activate());
    require(take(menu.snapshot()).opened&&!take(item->failure()),"A valid menu did not recover after icon resolution failed");
    good(host.clear_content());
    command->action=[&](const ListItemMenuSelection&){good(host.clear_content());};
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"retiring",input,{
            .menu_selection_triggered=[&](const ListItemMenuSelection&){++commands;}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    good(take(item->action(2)).activate());good(take(item->menu_item(3)).activate());const auto before=commands;
    good(take(item->menu_item({3,0})).activate());
    require(commands==before&&!item->root().snapshot(),"A retiring command delivered an outdated selection");
    auto source=std::make_shared<ReentrantIcons>();input.row.icon_source=source;input.menu_entries=std::vector<ListItemMenuEntry>{keep};
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"reentrant",input);
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    source->on_resolve=[&]{auto changed=take(item->options());ListItemMenuEntry fresh;fresh.label="Fresh";
        changed.menu_entries=std::vector<ListItemMenuEntry>{fresh};changed.row.icon_source.reset();good(item->set_options(changed));};
    good(take(item->action(2)).activate());
    require(take(item->options()).menu_entries->front().label=="Fresh"&&!take(take(item->action_menu()).snapshot()).opened&&
            !item->menu_item(0)&&!take(item->failure()),"Reentrant icon resolution published an obsolete menu or failure");
    good(take(item->action(2)).activate());require(take(take(item->menu_item(0)).snapshot()).text=="Fresh",
            "A newer menu could not open after reentrant icon resolution");
    auto earlier=take(item->menu_item(0));auto reconfigure=take(item->options());reconfigure.row.icon_source=source;
    reconfigure.menu_entries=std::vector<ListItemMenuEntry>{keep};good(item->set_options(reconfigure));
    source->on_resolve=[&]{auto changed=take(item->options());ListItemMenuEntry newer;newer.label="Newest";
        changed.menu_entries=std::vector<ListItemMenuEntry>{newer};changed.row.icon_source.reset();good(item->set_options(changed));};
    good(take(item->action(2)).activate());
    require(earlier.snapshot()&&take(take(item->menu_item(0)).snapshot()).text=="Fresh"&&
            !take(take(item->action_menu()).snapshot()).opened&&!take(item->failure()),
            "A stale menu candidate replaced the prior committed generation");
    good(take(item->action(2)).activate());require(!earlier.snapshot()&&take(take(item->menu_item(0)).snapshot()).text=="Newest",
            "A newer menu could not replace the preserved generation");
    auto lazy_options=take(item->options());ListItemMenuEntry lazy;lazy.label="Deferred";lazy.has_child_items=true;
    lazy_options.menu_entries=std::vector<ListItemMenuEntry>{lazy};good(item->set_options(lazy_options));
    good(take(item->action(2)).activate());good(take(item->menu_item(0)).activate());
    require(take(take(item->action_menu()).snapshot()).opened&&!item->menu_submenu({0}),
            "An authored child indicator did not preserve the menu while awaiting content");
    tree->close();
}
