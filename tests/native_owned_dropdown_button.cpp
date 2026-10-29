#include <LVUI/NativeDropdownButton.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
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
    for(const auto& run:frame.texts)if(run.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return true;
    return false;
}
bool has_fill(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    for(const auto& panel:frame.panels)if(panel.bounds==bounds&&panel.stroke_width==0)return true;
    for(const auto& layer:frame.overlays)for(const auto& panel:layer.panels)if(panel.bounds==bounds&&panel.stroke_width==0)return true;
    return false;
}
bool has_image(const ii::gui::CanvasFrame& frame,const std::shared_ptr<const ii::gui::RasterImage>& image){
    for(const auto& run:frame.images)if(run.image==image)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.images)if(run.image==image)return true;
    return false;
}
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{350,150}));std::optional<lvui::native::NativeDropdownButton> dropdown,icon;
    lvui::native::DropdownInput value;value.configuration.text="Open";std::vector<std::string> events;bool reenter=false;
    lvui::native::NativeDropdownCallbacks callbacks{.clicked=[&]{events.push_back("click");},.pressed=[&]{events.push_back("press");},
        .released=[&]{events.push_back("release");},.canceled=[&]{events.push_back("cancel");
            if(reenter){reenter=false;auto newer=value;newer.configuration.text="Reentered";good(dropdown->set_options(newer));}}};
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"layout",{.spacing=0,.alignment=StackAlignment::leading});if(!root)return std::unexpected(root.error());
        auto first=lvui::native::NativeDropdownButton::create(builder,*root,"dropdown",value,callbacks);if(!first)return std::unexpected(first.error());dropdown=*first;
        lvui::native::DropdownInput icon_value;icon_value.configuration.icon_mode=true;icon_value.configuration.left_padding=4;icon_value.configuration.spacing=-2;
        auto second=lvui::native::NativeDropdownButton::create(builder,*root,"icon",icon_value);if(!second)return std::unexpected(second.error());icon=*second;return {};
    }));
    auto frame=take(tree->drawing_snapshot({350,150}));auto snap=take(dropdown->root().snapshot());auto b=snap.bounds;
    require(snap.accessibility_name=="Open"&&has_text(frame,"Open")&&has_text(frame,"⌄")&&take(icon->root().snapshot()).bounds.width==40,
        "Dropdown appearance, default icon extent or accessibility changed");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=b.x+5,.y=b.y+5}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=b.x+5,.y=b.y+5}));
    require(events==std::vector<std::string>({"press","release","click"}),"Dropdown pointer callback order changed");
    events.clear();good(dropdown->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(events==std::vector<std::string>({"press","release","click"}),"Dropdown keyboard activation failed");
    events.clear();good(tree->input({.kind=WidgetInputKind::pointer_down,.x=b.x+5,.y=b.y+5}));
    auto disabling=value;disabling.configuration.enabled=false;reenter=true;good(dropdown->set_options(disabling));
    require(!reenter&&take(dropdown->options()).configuration.text=="Reentered"&&take(dropdown->options()).configuration.enabled&&
        std::ranges::find(events,"cancel")!=events.end(),"Dropdown cancellation overwrote a reentrant update");
    good(dropdown->set_options(value));
    auto image=take(rgba_image(1,1,std::vector<std::uint8_t>{255,255,255,255}));auto updated=take(icon->options());updated.icon=image;good(icon->set_options(updated));
    require(has_image(take(tree->drawing_snapshot({350,150})),image),"Application icon was not drawn");
    good(tree->input({.kind=WidgetInputKind::pointer_leave}));
    auto port=dropdown->weak_borderless_tone_port();good(port.apply(true));frame=take(tree->drawing_snapshot({350,150}));
    require(!has_fill(frame,b),"Dropdown did not adopt the temporary parent tone");
    value.configuration.tone=lvui::ButtonTone::destructive;value.configuration.width=120;good(dropdown->set_options(value));
    frame=take(tree->drawing_snapshot({350,150}));b=take(dropdown->root().snapshot()).bounds;
    require(b.width==120&&!has_fill(frame,b),"Reactive Dropdown options removed the temporary tone");
    good(port.apply(false));require(has_fill(take(tree->drawing_snapshot({350,150})),b),"Dropdown did not restore its authored tone");
    auto invalid=value;invalid.configuration.height=0;require(!dropdown->set_options(invalid)&&take(dropdown->options()).configuration.width==120,
        "Invalid Dropdown options replaced the live state");
    value.configuration.enabled=false;good(dropdown->set_options(value));events.clear();good(dropdown->root().activate());require(events.empty(),"Disabled Dropdown accepted activation");
    bool rejected=false;std::thread worker([&]{auto r=dropdown->set_options(value);rejected=!r&&r.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread changed Dropdown");good(host.clear_content());require(!port.apply(false)&&port.retired(),"Dropdown tone port retained a retired control");tree->close();
}
