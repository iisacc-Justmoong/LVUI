#include <LVUI/NativeComboBox.h>
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
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
bool has_text(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& run:frame.texts)if(run.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return true;
    return false;
}
void interactions(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,120}));std::optional<lvui::native::NativeComboBox> box;
    lvui::ComboBoxConfiguration config;config.text="Choice";std::vector<std::string> events;
    lvui::native::NativeComboBoxCallbacks callbacks;
    callbacks.clicked=[&]{events.push_back("click");};callbacks.pressed=[&]{events.push_back("press");};
    callbacks.released=[&]{events.push_back("release");};callbacks.canceled=[&]{
        events.push_back("cancel");auto newer=config;newer.text="Updated";good(box->set_options(newer));
    };
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=lvui::native::NativeComboBox::create(builder,{},"combo",config,callbacks);
        if(!created)return std::unexpected(created.error());box=*created;return {};
    }));
    auto frame=take(tree->drawing_snapshot({300,120}));auto snapshot=take(box->root().snapshot());
    require(has_text(frame,"Choice")&&has_text(frame,"↕")&&snapshot.action_role==ActionRole::button,
        "ComboBox did not draw its label and indicator with button semantics");
    auto b=snapshot.bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=b.x+5,.y=b.y+5}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=b.x+5,.y=b.y+5}));
    require(events==std::vector<std::string>({"press","release","click"}),"ComboBox pointer callback order changed");
    events.clear();good(box->root().focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(events==std::vector<std::string>({"press","release","click"}),"ComboBox keyboard activation failed");
    events.clear();good(tree->input({.kind=WidgetInputKind::pointer_down,.x=b.x+5,.y=b.y+5}));
    auto disabled=config;disabled.enabled=false;good(box->set_options(disabled));
    require(take(box->options()).enabled&&take(box->options()).text=="Updated"&&
        std::ranges::find(events,"cancel")!=events.end(),"Reentrant cancellation was overwritten");
    disabled.text="Disabled";good(box->set_options(disabled));events.clear();good(box->root().activate());
    require(events.empty(),"Disabled ComboBox accepted activation");
    auto bad=disabled;bad.width=0;require(!box->set_options(bad)&&take(box->options()).text=="Disabled",
        "Invalid ComboBox update replaced the live configuration");
    bool rejected=false;std::thread worker([&]{auto r=box->set_options(config);rejected=!r&&r.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread changed ComboBox");good(host.clear_content());
    require(!box->set_options(config),"Retired ComboBox accepted an update");tree->close();
}
void retire_in_callback(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,120}));std::optional<lvui::native::NativeComboBox> box;unsigned calls=0;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=lvui::native::NativeComboBox::create(builder,{},"combo",{},
            {.clicked=[&]{++calls;good(host.clear_content());}});
        if(!created)return std::unexpected(created.error());box=*created;return {};
    }));
    good(box->root().activate());require(calls==1&&!box->root().snapshot(),"ComboBox did not survive retirement in its callback");tree->close();
}
}
int main(){interactions();retire_in_callback();}
