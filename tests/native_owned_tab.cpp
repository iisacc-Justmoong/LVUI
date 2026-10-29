#include <LVUI/NativeTab.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool has_text(const ii::gui::CanvasFrame& frame,std::string_view text){
    const auto in=[&](const auto& values){return std::ranges::any_of(values,[&](const auto& value){return value.text==text;});};
    if(in(frame.texts))return true;for(const auto& layer:frame.overlays)if(in(layer.texts))return true;return false;
}
bool has_icon(const ii::gui::CanvasFrame& frame){
    if(!frame.images.empty())return true;for(const auto& layer:frame.overlays)if(!layer.images.empty())return true;return false;
}
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
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,120}));std::optional<lvui::native::NativeTab> tab;
    lvui::native::TabInput input;input.configuration.icon_name="home";input.configuration.text="Overview";
    input.configuration.badge="3";input.configuration.selected=true;int clicks=0;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto created=lvui::native::NativeTab::create(builder,*stack,"tab",input,[&]{++clicks;});
        if(!created)return std::unexpected(created.error());tab=*created;return {};
    }));
    auto snapshot=take(tab->root().snapshot());auto frame=take(tree->drawing_snapshot({320,120}));
    if(!(snapshot.text=="Overview"&&snapshot.accessibility_name=="Overview"&&snapshot.action_role==ActionRole::tab&&snapshot.selected&&
         snapshot.bounds.width>=80&&snapshot.bounds.height==32&&has_text(frame,"Overview")&&has_text(frame,"3")&&has_icon(frame)))
        throw std::runtime_error("Tab appearance: text="+snapshot.text+" name="+snapshot.accessibility_name+
            " selected="+std::to_string(snapshot.selected)+" width="+std::to_string(snapshot.bounds.width)+
            " height="+std::to_string(snapshot.bounds.height)+" label="+std::to_string(has_text(frame,"Overview"))+
            " badge="+std::to_string(has_text(frame,"3"))+" icon="+std::to_string(has_icon(frame)));
    good(tab->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));require(clicks==1,"Tab keyboard activation failed");
    auto updated=input;updated.configuration.text="Details";updated.configuration.selected=false;updated.configuration.style=lvui::TabStyle::surface;
    good(tab->set_options(updated));snapshot=take(tab->root().snapshot());
    require(snapshot.text=="Details"&&!snapshot.selected&&has_text(take(tree->drawing_snapshot({320,120})),"Details"),
            "Tab did not apply the new configuration");
    auto missing=updated;missing.configuration.icon_name="missing";
    require(!tab->set_options(missing)&&take(tab->options())==updated&&take(tab->root().snapshot()).text=="Details",
            "An unknown icon replaced the live tab");
    auto source=std::make_shared<Reentrant>();source->callback=[&]{auto newer=updated;newer.configuration.text="Newest";good(tab->set_options(newer));};
    auto older=updated;older.configuration.text="Older";older.configuration.icon_name="custom";older.icon_source=source;
    good(tab->set_options(older));require(take(tab->options()).configuration.text=="Newest","Icon source overwrote a reentrant tab update");
    auto port=tab->weak_group_port();good(port.present({.selected=true,.width=140}));
    require(take(tab->root().snapshot()).selected&&take(tab->root().snapshot()).bounds.width==140&&
            !take(tab->options()).configuration.selected,"Parent presentation changed authored Tab options");
    int grouped=0;good(port.activated([&]{++grouped;return false;}));good(tab->root().activate());
    require(grouped==1&&clicks==1,"Rejected group activation reached the tab action");
    good(port.activated({}));good(port.activated([&]{++grouped;return true;}));good(tab->root().activate());
    require(grouped==2&&clicks==2,"Group activation did not precede the tab action");
    good(port.present({}));require(!take(tab->root().snapshot()).selected,"Clearing group presentation did not restore authored selection");
    updated=take(tab->options());updated.configuration.enabled=false;good(tab->set_options(updated));good(tab->root().activate());
    require(clicks==2&&!take(tab->root().snapshot()).enabled,"Disabled tab accepted activation");
    bool rejected=false;std::thread worker([&]{auto r=tab->set_options(updated);rejected=!r&&r.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread changed a tab");good(host.clear_content());
    require(!tab->set_options(updated)&&port.retired()&&!port.present({}),"Retired tab or its group port accepted an update");tree->close();
}
