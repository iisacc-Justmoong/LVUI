#include <LVUI/NativeSegmentedControl.h>
#include <iiGUI/NativeSegmentedControlView.h>
#include <LVUI/NativeButton.h>
#include <iiGUI/NativeButtonView.h>
#include <iiGUI/SvgImageCache.h>
#include <iiGUI/WidgetState.h>
#include <memory>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>
namespace lvui::native {
using ButtonEvent=ButtonEventValue<ii::gui::Widget>;
using ButtonMethod=ButtonMethodValue<ButtonEvent>;
using ButtonMethodRegistry=ButtonMethodRegistryValue<ButtonEvent>;
using ButtonInput=ButtonInputValue<ii::gui::RasterImage>;
using ButtonModel=ii::gui::BasicButton<ButtonHostModelPolicy>;
using NativeButton=ButtonBinding<ii::gui::BasicButton,ii::gui::BasicNativeButton,ii::gui::CachedSvgDecoder,ii::gui::Widget>;
}
namespace lvui::native {
using NativeSegmentedControl=SegmentedControlBinding<ii::gui::BasicSegmentedControl,ii::gui::BasicNativeSegmentedControl,
    ButtonMethod,ButtonMethodRegistry,ButtonMethodInvoker>;
}
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
bool has_fill(const ii::gui::CanvasFrame& frame,const ii::gui::Rect& bounds){
    for(const auto& panel:frame.panels)if(panel.bounds==bounds&&panel.stroke_width==0)return true;
    for(const auto& layer:frame.overlays)for(const auto& panel:layer.panels)if(panel.bounds==bounds&&panel.stroke_width==0)return true;
    return false;
}
void design_geometry(){
    using Model=ii::gui::BasicSegmentedControl<lvui::native::SegmentedControlModelPolicy>;
    Model row;const auto original=row.configuration();auto rect=row.geometry(100,30);
    require(rect&&rect->radius==8&&rect->border_width==0,"LVUI rounded segment geometry changed");
    auto capsule=original;capsule.shape=lvui::SegmentedShape::cylinder;capsule.border_width=2;require(row.set_configuration(capsule),"LVUI capsule rejected");
    auto cylinder=row.geometry(100,30);auto narrow=row.geometry(8,30);
    require(cylinder&&cylinder->radius==15&&narrow&&narrow->radius==4&&narrow->border_width==2,"LVUI capsule geometry changed");
    require(!row.geometry(-1,30)&&!row.geometry(std::numeric_limits<double>::infinity(),30),"LVUI invalid geometry accepted");
    auto invalid=capsule;invalid.spacing=-1;require(!row.set_configuration(invalid)&&row.configuration()==capsule,"LVUI invalid state replaced");
    invalid=capsule;invalid.shape=static_cast<lvui::SegmentedShape>(8);require(!row.set_configuration(invalid),"LVUI invalid shape accepted");
    invalid=capsule;invalid.orientation=static_cast<lvui::SegmentedOrientation>(8);require(!row.set_configuration(invalid),"LVUI invalid orientation accepted");
    invalid=capsule;invalid.border.alpha=-.1;require(!row.set_configuration(invalid),"LVUI invalid color accepted");
}
void manual_methods(){
    using namespace ii::gui;using lvui::native::ButtonEvent;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"methods-host",{240,120}));
    std::optional<lvui::native::NativeSegmentedControl> group;std::optional<lvui::native::NativeButton> child;
    std::vector<std::string> events;lvui::SegmentedControlConfiguration config;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=take(builder.vstack({},"root"));group=take(lvui::native::NativeSegmentedControl::create(builder,root,"group",config));
        child=take(lvui::native::NativeButton::create(builder,group->root(),"child",{},[&]{events.emplace_back("child");}));return {};
    }));
    good(group->set_method([&](const ButtonEvent& event){
        require(event.source.snapshot().has_value()&&!event.tone&&event.trigger=="manual"&&event.enabled&&event.effective_enabled,
                "Segmented manual event is invalid");events.emplace_back("method");}));
    good(group->set_methods({[&](const auto&){events.emplace_back("first");},[&](const auto&){events.emplace_back("second");}}));
    require(take(group->has_methods()),"Segmented methods were not retained");good(child->root().activate());
    require(events==std::vector<std::string>{"child"},"A child click automatically invoked the group methods");
    events.clear();good(group->invoke_methods());
    require(events==std::vector<std::string>{"method","first","second"},"Segmented manual methods ran out of order");
    config.enabled=false;good(group->set_options(config));events.clear();std::optional<ButtonEvent> retained_event;
    good(group->set_method([&](const ButtonEvent& event){require(!event.enabled&&!event.effective_enabled&&event.trigger=="custom",
        "Disabled segmented manual event lost its state");retained_event=event;events.emplace_back("disabled");}));
    std::string trigger="custom";good(group->invoke_methods(trigger));trigger.assign("mutate");
    require(retained_event&&retained_event->trigger=="custom","Segmented event retained a borrowed trigger");
    require(events==std::vector<std::string>{"disabled","first","second"},
        "Disabled segmented control rejected manual invocation");
    bool foreign=false;std::thread worker([&]{auto result=group->set_method({});foreign=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});
    worker.join();require(foreign,"Segmented methods accepted a foreign thread");events.clear();
    good(group->set_method([&](const auto&){events.emplace_back("retire");good(host.clear_content());}));
    good(group->invoke_methods());require(events==std::vector<std::string>{"retire"},"Retired segmented control invoked later methods");
    require(!group->set_methods({}),"Retired segmented control accepted methods");tree->close();
}
void vertical_fifteen(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{100,500}));
    std::optional<lvui::native::NativeSegmentedControl> group;
    std::vector<Widget> children;
    lvui::SegmentedControlConfiguration config;config.orientation=lvui::SegmentedOrientation::vertical;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"root");if(!root)return std::unexpected(root.error());
        auto value=lvui::native::NativeSegmentedControl::create(builder,*root,"vertical",config);if(!value)return std::unexpected(value.error());group=*value;
        for(int i=0;i<15;++i){auto child=builder.action_surface(group->root(),"item"+std::to_string(i),{.text="",.width=22,.height=22},{});if(!child)return std::unexpected(child.error());children.push_back(*child);}return {};
    }));
    (void)take(tree->drawing_snapshot({100,500}));
    const auto groupBounds=take(group->root().snapshot()).bounds;
    if(groupBounds.width!=22 || groupBounds.height!=358)throw std::runtime_error("Vertical group extent: "+std::to_string(groupBounds.width)+"x"+std::to_string(groupBounds.height));
    for(int i=0;i<15;++i){const auto bounds=take(children[i].snapshot()).bounds;
        require(bounds.x==groupBounds.x && bounds.y==groupBounds.y+i*24,"Vertical segment position is incorrect");}
    auto changed=config;changed.orientation=lvui::SegmentedOrientation::horizontal;
    require(!group->set_options(changed) && take(group->options()).orientation==config.orientation,"Orientation change must preserve the original layout");
    tree->close();
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
    authored.configuration.checkable=true;good(button->set_options(authored));
    auto frame=take(tree->drawing_snapshot({300,100}));auto bounds=take(button->root().snapshot()).bounds;
    good(button->root().activate());require(take(button->root().snapshot()).checked,"Segment button did not toggle checked");
    frame=take(tree->drawing_snapshot({300,100}));require(has_fill(frame,bounds),"Selected segment did not draw a selected background");
    authored.configuration.palette.selected=lvui::StyleColor{.8,.1,.2,1};good(button->set_options(authored));
    require(take(button->root().snapshot()).checked,"Accent update reset the selected segment");
    frame=take(tree->drawing_snapshot({300,100}));bool accent=false;
    const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    const auto inspect=[&](const auto& panels){for(const auto& panel:panels)if(panel.bounds==bounds && panel.stroke_width==0 &&
        std::abs(panel.top.red-linear(.8))<1e-5 && std::abs(panel.top.green-linear(.1))<1e-5)accent=true;};
    inspect(frame.panels);for(const auto& layer:frame.overlays)inspect(layer.panels);
    require(accent,"Selected segment did not follow the app accent");
    require(take(button->root().snapshot()).tone==ii::gui::ButtonTone::normal,"Selection changed the public button tone");
    good(button->root().activate());require(!take(button->root().snapshot()).checked,"Segment button did not toggle off");
    frame=take(tree->drawing_snapshot({300,100}));
    require(!has_fill(frame,bounds),"Segmented parent failed to apply its borderless tone");
    authored.configuration.tone=lvui::ButtonTone::destructive;good(button->set_options(authored));
    require(take(button->options()).configuration.tone==lvui::ButtonTone::destructive,
            "A temporary segment style overwrote the authored button options");
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
    design_geometry();
    manual_methods();
    vertical_fifteen();
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,160}));std::optional<lvui::native::NativeSegmentedControl> row;
    Widget first,second;unsigned clicks=0;lvui::SegmentedControlConfiguration config;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"root");if(!stack)return std::unexpected(stack.error());
        auto created=lvui::native::NativeSegmentedControl::create(builder,*stack,"segments",config);if(!created)return std::unexpected(created.error());row=*created;
        auto a=builder.action_surface(row->root(),"first",{.text="A",.width=40,.height=22},[&]{++clicks;});if(!a)return std::unexpected(a.error());first=*a;
        auto b=builder.action_surface(row->root(),"second",{.text="B",.width=40,.height=22},[&]{++clicks;});if(!b)return std::unexpected(b.error());second=*b;
        return {};
    }));
    auto frame=take(tree->drawing_snapshot({400,160}));auto snapshot=take(row->root().snapshot());
    std::size_t panels=frame.panels.size();for(const auto& layer:frame.overlays)panels+=layer.panels.size();
    if(!has_fill(frame,snapshot.bounds)||snapshot.bounds.width!=82||take(first.snapshot()).bounds.width!=40)
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
