#include <LVUI/NativeSegmentedControl.h>
#include <LVUI/NativeButton.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
bool has_fill(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    for(const auto& panel:frame.panels)if(panel.bounds==bounds&&panel.stroke_width==0)return true;
    for(const auto& layer:frame.overlays)for(const auto& panel:layer.panels)if(panel.bounds==bounds&&panel.stroke_width==0)return true;
    return false;
}
void button_tone_port(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeSegmentedControl> row;std::optional<lvui::native::NativeButton> button;
    lvui::native::ButtonInput authored;authored.configuration.text="Select";
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto group=lvui::native::NativeSegmentedControl::create(builder,{},"group");if(!group)return std::unexpected(group.error());row=*group;
        auto child=lvui::native::NativeButton::create(builder,row->root(),"button",authored);if(!child)return std::unexpected(child.error());button=*child;
        return row->register_tone_sink(button->weak_borderless_tone_port());
    }));
    auto frame=take(tree->drawing_snapshot({300,100}));auto bounds=take(button->root().snapshot()).bounds;
    require(!has_fill(frame,bounds),"Segmented parent failed to apply its borderless tone");
    authored.configuration.tone=lvui::ButtonTone::destructive;good(button->set_options(authored));
    frame=take(tree->drawing_snapshot({300,100}));require(!has_fill(frame,bounds),"Reactive child options removed the temporary tone");
    auto options=take(row->options());options.force_borderless_tone=false;good(row->set_options(options));
    frame=take(tree->drawing_snapshot({300,100}));require(has_fill(frame,bounds),"Removing the temporary tone lost the authored tone");
    options.force_borderless_tone=true;good(row->set_options(options));authored.configuration.tone=lvui::ButtonTone::disabled;
    good(button->set_options(authored));require(!take(button->root().snapshot()).enabled,"Segmented tone override enabled a disabled child");
    auto port=button->weak_borderless_tone_port();good(host.clear_content());require(!port.apply(false)&&port.retired(),"Tone port retained a retired child");tree->close();
}
void prune_retired_child(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,100}));
    std::optional<lvui::native::NativeSegmentedControl> row;std::optional<lvui::native::NativeButton> button;Widget slot;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto group=lvui::native::NativeSegmentedControl::create(builder,{},"group");if(!group)return std::unexpected(group.error());row=*group;
        auto child_host=builder.content_host(row->root(),"slot",{100,30});if(!child_host)return std::unexpected(child_host.error());slot=*child_host;
        auto created=slot.set_content([&](auto& nested)->ii::core::Result<void>{
            lvui::native::ButtonInput input;input.configuration.text="Child";
            auto child=lvui::native::NativeButton::create(nested,{},"button",input);if(!child)return std::unexpected(child.error());button=*child;return {};
        });
        if(!created)return created;return row->register_tone_sink(button->weak_borderless_tone_port());
    }));
    require(take(row->prune_retired_tone_sinks())==0,"A live child port was removed");
    good(slot.clear_content());require(take(row->prune_retired_tone_sinks())==1&&take(row->prune_retired_tone_sinks())==0,
        "Retired child style port was not removed exactly once");
    auto config=take(row->options());config.force_borderless_tone=false;good(row->set_options(config));tree->close();
}
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,160}));std::optional<lvui::native::NativeSegmentedControl> row;
    Widget first,second;unsigned clicks=0;lvui::SegmentedControlConfiguration config;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto created=lvui::native::NativeSegmentedControl::create(builder,{},"segments",config);if(!created)return std::unexpected(created.error());row=*created;
        auto a=builder.action_surface(row->root(),"first",{.text="A",.width=40,.height=22},[&]{++clicks;});if(!a)return std::unexpected(a.error());first=*a;
        auto b=builder.action_surface(row->root(),"second",{.text="B",.width=40,.height=22},[&]{++clicks;});if(!b)return std::unexpected(b.error());second=*b;
        return {};
    }));
    auto frame=take(tree->drawing_snapshot({400,160}));auto snapshot=take(row->root().snapshot());
    std::size_t panels=frame.panels.size();for(const auto& layer:frame.overlays)panels+=layer.panels.size();
    if(panels<2||snapshot.bounds.width<90||take(first.snapshot()).bounds.width!=40)
        throw std::runtime_error("Segmented layout: panels="+std::to_string(panels)+" group width="+std::to_string(snapshot.bounds.width)+
            " child width="+std::to_string(take(first.snapshot()).bounds.width));
    good(first.activate());good(second.activate());require(clicks==2,"Segmented control replaced its children's actions");
    bool a=false,b=false,fail=false,closed=false,reenter=false;
    good(row->register_tone_sink([&](bool value)->ii::core::Result<void>{
        a=value;if(reenter){reenter=false;auto newest=config;newest.spacing=9;newest.force_borderless_tone=value;good(row->set_options(newest));}return {};
    }));
    good(row->register_tone_sink([&](bool value)->ii::core::Result<void>{
        if(fail&&!value)return ii::core::fail(ii::core::ErrorCode::failed,"Rejected child tone");b=value;return {};
    }));
    good(row->register_tone_sink([&](bool)->ii::core::Result<void>{if(closed)return ii::core::fail(ii::core::ErrorCode::closed,"retired");return {};}));
    require(a&&b,"Initial segmented style did not reach children");
    auto next=config;next.force_borderless_tone=false;fail=true;
    require(!row->set_options(next)&&a&&b&&take(row->options()).force_borderless_tone,
        "A rejected child style did not restore the prior group and children");
    fail=false;closed=true;good(row->set_options(next));require(!a&&!b&&!take(row->options()).force_borderless_tone,
        "Segmented style update did not reach live children");
    reenter=true;auto older=config;older.spacing=1;good(row->set_options(older));
    require(take(row->options()).spacing==9&&take(row->options()).force_borderless_tone&&a&&b,
        "Older segmented options overwrote a reentrant update");
    config=take(row->options());config.enabled=false;good(row->set_options(config));good(first.activate());
    require(clicks==2,"A disabled segmented parent allowed child activation");
    auto bad=config;bad.spacing=-1;require(!row->set_options(bad)&&take(row->options()).spacing==9,
        "Invalid segmented options replaced valid state");
    bool rejected=false;std::thread worker([&]{auto r=row->set_options(config);rejected=!r&&r.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread changed segmented control");good(host.clear_content());
    require(!row->set_options(config),"Retired segmented control accepted updates");tree->close();button_tone_port();prune_retired_child();
}
