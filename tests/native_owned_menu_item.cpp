#include <LVUI/NativeMenuItem.h>
#include <iiGUI/NativeMenuItemView.h>
#include <iiGUI/SvgImageCache.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace lvui::native {
using MenuItemInput=MenuItemInputValue<ii::gui::RasterImage>;
using MenuPopupPort=MenuPopupPortValue<ii::gui::Widget,ii::gui::WidgetSnapshot>;
using NativeMenuItem=MenuItemBinding<ii::gui::BasicMenuItem,ii::gui::BasicNativeMenuItem,ii::gui::CachedSvgDecoder,ii::gui::Widget,ii::gui::WidgetSnapshot>;
}
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool has_text(const ii::gui::CanvasFrame& frame,std::string_view text){
    const auto in=[&](const auto& values){return std::ranges::any_of(values,[&](const auto& value){return value.text==text;});};
    if(in(frame.texts))return true;for(const auto& layer:frame.overlays)if(in(layer.texts))return true;return false;
}
std::size_t images(const ii::gui::CanvasFrame& frame){std::size_t count=frame.images.size();for(const auto& layer:frame.overlays)count+=layer.images.size();return count;}
struct Reentrant final:lvui::IconSource {
    mutable std::function<void()> callback;
    lvui::IconResult<std::shared_ptr<const lvui::IconAsset>> resolve(std::string_view)const override{
        auto call=std::move(callback);if(call)call();
        return std::make_shared<const lvui::IconAsset>(lvui::IconAsset{
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 10 10'><rect width='10' height='10'/></svg>"});
    }
    std::uint64_t revision()const noexcept override{return 1;}
};
struct ThrowingCopy final {
    std::shared_ptr<bool> enabled;
    explicit ThrowingCopy(std::shared_ptr<bool> value):enabled(std::move(value)){}
    ThrowingCopy(const ThrowingCopy& other):enabled(other.enabled){if(*enabled)throw std::runtime_error("Observer copy failed");}
    ThrowingCopy(ThrowingCopy&&)=default;
    ii::core::Result<void> operator()()const{return {};}
};
void retired_item_releases_callbacks(){
    using namespace ii::gui;using namespace lvui::native;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,160}));std::optional<NativeMenuItem> retained;
    auto payload=std::make_shared<int>(7);std::weak_ptr<int> weak=payload;
    auto provider=std::make_shared<Reentrant>();std::weak_ptr<Reentrant> weak_provider=provider;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        MenuItemInput input;input.configuration.icon_name="custom";input.icon_source=provider;input.configuration.show_chevron=false;
        NativeMenuItemCallbacks callbacks;callbacks.clicked=[payload]{};
        auto result=NativeMenuItem::create(builder,{},"item",input,std::move(callbacks));
        if(!result)return std::unexpected(result.error());retained=*result;return {};
    }));
    payload.reset();provider.reset();require(!weak.expired()&&!weak_provider.expired(),"Live item failed to retain its resources");
    good(host.clear_content());
    require(weak.expired()&&weak_provider.expired(),"Retired item retained callback or icon resources through its public handle");
    tree->close();
}
void retired_during_icon_resolution(){
    using namespace ii::gui;using namespace lvui::native;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,160}));std::optional<NativeMenuItem> item;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        MenuItemInput input;input.configuration.icon_name.clear();input.configuration.show_icon_slot=false;
        auto result=NativeMenuItem::create(builder,{},"item",input);if(!result)return std::unexpected(result.error());item=*result;return {};
    }));
    auto source=std::make_shared<Reentrant>();source->callback=[&]{good(host.clear_content());};
    auto candidate=take(item->options());candidate.configuration.icon_name="custom";candidate.configuration.show_icon_slot=true;candidate.icon_source=source;
    auto result=item->set_options(candidate);
    require(!result&&result.error().code==ii::core::ErrorCode::closed,"Retirement during icon resolution was not cancelled");
    tree->close();
}
void submenu_attachment_rollback(){
    using namespace ii::gui;using namespace lvui::native;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,160}));std::optional<NativeMenuItem> item,newer_item,copy_item,reentrant_item,retired_item;
    Widget popup,newer_popup,copy_popup,reentrant_popup,retired_popup;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"root");if(!root)return std::unexpected(root.error());
        MenuItemInput input;input.configuration.label="Owner";input.configuration.show_icon_slot=false;input.configuration.show_chevron=false;
        auto owner=NativeMenuItem::create(builder,*root,"owner",input);if(!owner)return std::unexpected(owner.error());item=*owner;
        auto child=builder.popover(item->root(),"child");if(!child)return std::unexpected(child.error());popup=*child;
        auto newer=NativeMenuItem::create(builder,*root,"newer",input);if(!newer)return std::unexpected(newer.error());newer_item=*newer;
        auto second=builder.popover(newer_item->root(),"newer-child");if(!second)return std::unexpected(second.error());newer_popup=*second;
        auto copying=NativeMenuItem::create(builder,*root,"copying",input);if(!copying)return std::unexpected(copying.error());copy_item=*copying;
        auto third=builder.popover(copy_item->root(),"copy-child");if(!third)return std::unexpected(third.error());copy_popup=*third;
        auto reentrant=NativeMenuItem::create(builder,*root,"reentrant",input);if(!reentrant)return std::unexpected(reentrant.error());reentrant_item=*reentrant;
        auto fourth=builder.popover(reentrant_item->root(),"reentrant-child");if(!fourth)return std::unexpected(fourth.error());reentrant_popup=*fourth;
        auto retired=NativeMenuItem::create(builder,*root,"retired",input);if(!retired)return std::unexpected(retired.error());retired_item=*retired;
        auto fifth=builder.popover(retired_item->root(),"retired-child");if(!fifth)return std::unexpected(fifth.error());retired_popup=*fifth;return {};
    }));
    auto group=item->weak_group_port();
    require(take(group.entry()).kind==lvui::MenuEntryKind::action,"Hidden chevron did not begin as a leaf");
    good(group.changed([]()->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::busy,"Rejected by menu");}));
    MenuPopupPort adapter{.snapshot=[popup]{return popup.snapshot();},
        .open=[](Widget,lvui::MenuDirection,MenuCloseAction)->ii::core::Result<void>{return {};}};
    auto rejected=item->set_submenu(adapter);
    require(!rejected&&rejected.error().code==ii::core::ErrorCode::busy&&take(group.entry()).kind==lvui::MenuEntryKind::action,
        "A rejected submenu attachment changed the retained menu entry");
    good(group.changed({}));
    good(group.changed([]()->ii::core::Result<void>{throw std::runtime_error("Observer threw");}));
    auto thrown=item->set_submenu(adapter);
    require(!thrown&&take(group.entry()).kind==lvui::MenuEntryKind::action,
        "A throwing menu observer left a partially attached submenu");
    good(group.changed({}));good(item->set_submenu(std::move(adapter)));
    require(take(group.entry()).kind==lvui::MenuEntryKind::submenu,"A valid submenu did not attach after rollback");
    auto newer_group=newer_item->weak_group_port();bool first=true;
    good(newer_group.changed([&]()->ii::core::Result<void>{
        if(!first)return {};
        first=false;auto updated=take(newer_item->options());updated.configuration.label="Newer";
        if(auto result=newer_item->set_options(updated);!result)return result;
        return ii::core::fail(ii::core::ErrorCode::busy,"Older update failed");
    }));
    MenuPopupPort newer_adapter{.snapshot=[newer_popup]{return newer_popup.snapshot();},
        .open=[](Widget,lvui::MenuDirection,MenuCloseAction)->ii::core::Result<void>{return {};}};
    auto older=newer_item->set_submenu(std::move(newer_adapter));
    require(!older&&take(newer_item->options()).configuration.label=="Newer"&&take(newer_group.entry()).kind==lvui::MenuEntryKind::submenu,
        "An older failed attachment replaced a newer reentrant update");
    auto copy_group=copy_item->weak_group_port();auto throw_on_copy=std::make_shared<bool>(false);
    std::function<ii::core::Result<void>()> observer=ThrowingCopy{throw_on_copy};good(copy_group.changed(std::move(observer)));*throw_on_copy=true;
    MenuPopupPort copy_adapter{.snapshot=[copy_popup]{return copy_popup.snapshot();},
        .open=[](Widget,lvui::MenuDirection,MenuCloseAction)->ii::core::Result<void>{return {};}};
    auto copy_failure=copy_item->set_submenu(copy_adapter);
    require(!copy_failure&&take(copy_group.entry()).kind==lvui::MenuEntryKind::action,
        "A failed observer copy left a partially attached submenu");
    *throw_on_copy=false;good(copy_group.changed({}));good(copy_item->set_submenu(std::move(copy_adapter)));
    int inner_opens=0,outer_opens=0;bool first_snapshot=true;
    MenuPopupPort inner{.snapshot=[reentrant_popup]{return reentrant_popup.snapshot();},
        .open=[&](Widget,lvui::MenuDirection,MenuCloseAction)->ii::core::Result<void>{++inner_opens;return {};}};
    MenuPopupPort outer{.snapshot=[&]()->ii::core::Result<WidgetSnapshot>{if(first_snapshot){first_snapshot=false;good(reentrant_item->set_submenu(inner));}return reentrant_popup.snapshot();},
        .open=[&](Widget,lvui::MenuDirection,MenuCloseAction)->ii::core::Result<void>{++outer_opens;return {};}};
    auto stale=reentrant_item->set_submenu(std::move(outer));
    require(!stale&&take(reentrant_item->weak_group_port().open_submenu())&&inner_opens==1&&outer_opens==0,
        "A stale submenu attachment replaced a newer reentrant attachment");
    good(host.clear_content());int snapshot_calls=0;
    MenuPopupPort retired_adapter{.snapshot=[&]()->ii::core::Result<WidgetSnapshot>{++snapshot_calls;return retired_popup.snapshot();},
        .open=[](Widget,lvui::MenuDirection,MenuCloseAction)->ii::core::Result<void>{return {};}};
    require(!retired_item->set_submenu(std::move(retired_adapter))&&snapshot_calls==0,
        "A retired menu item invoked an external popup adapter");
    tree->close();
}
}
int main(){
    retired_item_releases_callbacks();
    retired_during_icon_resolution();
    using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,120}));std::optional<lvui::native::NativeMenuItem> item;
    lvui::native::MenuItemInput input;input.configuration.label="Open";input.configuration.shortcut="⌘O";
    std::vector<std::string> events;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto created=lvui::native::NativeMenuItem::create(builder,*stack,"item",input,
            {.clicked=[&]{events.push_back("click");},.pressed=[&]{events.push_back("press");},.released=[&]{events.push_back("release");},.canceled=[&]{events.push_back("cancel");}});
        if(!created)return std::unexpected(created.error());item=*created;return {};
    }));
    auto snapshot=take(item->root().snapshot());auto frame=take(tree->drawing_snapshot({320,120}));
    require(snapshot.action_role==ActionRole::menu_item&&snapshot.accessibility_name=="Open"&&snapshot.accessibility_description=="⌘O"&&
            snapshot.bounds.width>=161&&snapshot.bounds.height==24&&has_text(frame,"Open")&&has_text(frame,"⌘O")&&images(frame)>=2,
            "Menu item appearance, icon or accessibility is incorrect");
    const auto x=snapshot.bounds.x+5,y=snapshot.bounds.y+5;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y}));good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));
    require(events==std::vector<std::string>({"press","release","click"}),"Menu item pointer event order changed");
    events.clear();good(item->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(events==std::vector<std::string>({"press","release","click"}),"Menu item keyboard activation failed");
    auto selected=input;selected.configuration.state=lvui::MenuEntryState::selected;selected.configuration.expanded=true;
    good(item->set_options(selected));require(take(item->root().snapshot()).selected,"Menu item selected state was not exposed");
    auto port=item->weak_group_port();good(port.present({.selected=false,.width=200}));
    require(!take(item->root().snapshot()).selected&&take(item->root().snapshot()).bounds.width>=200&&
            take(item->options()).configuration.state==lvui::MenuEntryState::selected,"Menu presentation changed authored options");
    good(port.present({}));require(take(item->root().snapshot()).selected,"Menu item did not restore authored selection");
    events.clear();good(port.activated([&](std::function<void()> callback){events.push_back("group");if(callback)callback();}));
    good(item->root().activate());require(events==std::vector<std::string>({"group","click"}),"Menu did not control item activation order");
    good(port.activated({}));int changes=0;good(port.changed([&]()->ii::core::Result<void>{++changes;return {};}));good(item->set_options(selected));
    require(changes==1&&take(port.entry()).label=="Open","Menu item did not publish its authored change");good(port.changed({}));
    auto bad=selected;bad.configuration.icon_name="missing";require(!item->set_options(bad)&&take(item->options())==selected,
        "Missing menu item icon changed the active state");
    auto source=std::make_shared<Reentrant>();source->callback=[&]{auto newer=selected;newer.configuration.label="Newest";good(item->set_options(newer));};
    auto older=selected;older.configuration.label="Older";older.configuration.icon_name="custom";older.icon_source=source;
    good(item->set_options(older));require(take(item->options()).configuration.label=="Newest","Icon provider overwrote a reentrant menu item update");
    auto inactive=take(item->options());inactive.configuration.state=lvui::MenuEntryState::inactive;good(item->set_options(inactive));events.clear();
    good(item->root().activate());require(events.empty()&&!take(item->root().snapshot()).enabled,"Inactive menu item accepted activation");
    bool rejected=false;std::thread worker([&]{auto r=item->set_options(inactive);rejected=!r&&r.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread changed a menu item");good(host.clear_content());
    require(!item->set_options(inactive)&&port.retired()&&!port.present({}),"Retired menu item or port accepted an update");tree->close();
    submenu_attachment_rollback();
}
