#include <LVUI/NativeMenuItem.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
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
}
int main(){
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
}
