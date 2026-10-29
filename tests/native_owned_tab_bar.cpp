#include <LVUI/NativeTabBar.h>
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
}
int main(){
    using namespace ii::gui;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{480,120}));
    std::optional<lvui::native::NativeTabBar> bar;std::optional<lvui::native::NativeTab> first,disabled,last;
    std::vector<int> selections;int clicks=0;bool fail=false,throw_on_failure=false,reenter=false;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::stretch});if(!stack)return std::unexpected(stack.error());
        lvui::TabBarConfiguration bar_config;bar_config.accessibility_name="Sections";
        auto created=lvui::native::NativeTabBar::create(builder,*stack,"bar",bar_config,[&](int index){
            selections.push_back(index);if(reenter){reenter=false;auto latest=take(bar->options());latest.current_index=0;good(bar->set_options(latest));}});
        if(!created)return std::unexpected(created.error());bar=*created;
        lvui::native::TabInput value;value.configuration.text="First";value.configuration.icon_name="home";
        auto a=lvui::native::NativeTab::create(builder,bar->root(),"first",value,[&]{++clicks;});if(!a)return std::unexpected(a.error());first=*a;
        if(auto r=bar->register_tab(first->weak_group_port());!r)return r;
        value.configuration.text="Disabled";value.configuration.enabled=false;
        auto b=lvui::native::NativeTab::create(builder,bar->root(),"disabled",value);if(!b)return std::unexpected(b.error());disabled=*b;
        if(auto r=bar->register_tab(disabled->weak_group_port());!r)return r;
        value.configuration.text="Last";value.configuration.icon_name="generaladd";value.configuration.enabled=true;
        auto c=lvui::native::NativeTab::create(builder,bar->root(),"last",value,[&]{++clicks;});if(!c)return std::unexpected(c.error());last=*c;
        auto port=last->weak_group_port();auto present=port.present;
        port.present=[&fail,&throw_on_failure,present](lvui::native::TabPresentation value)->ii::core::Result<void>{
            if(fail&&value.selected==true){if(throw_on_failure)throw std::runtime_error("Thrown child failure");
                return ii::core::fail(ii::core::ErrorCode::failed,"Rejected selection");}return present(value);};
        if(auto r=bar->register_tab(std::move(port));!r)return r;
        return bar->finalize();
    }));
    (void)take(tree->drawing_snapshot({480,120}));good(bar->status());
    auto a=take(first->root().snapshot()),b=take(disabled->root().snapshot()),c=take(last->root().snapshot());
    auto group=take(bar->root().snapshot());
    require(group.accessibility_semantics&&group.accessibility_semantics->role==AccessibilityRole::tab_group&&group.accessibility_name=="Sections"&&
            a.selected&&!b.selected&&!c.selected&&std::abs(a.bounds.width-b.bounds.width)<.01&&std::abs(a.bounds.width-c.bounds.width)<.01,
            "TabBar initial selection or equal widths are incorrect");
    good(bar->root().set_extent({480,40}));(void)take(tree->drawing_snapshot({480,120}));
    const auto full_width=take(first->root().snapshot()).bounds.width;auto hidden=take(last->options());hidden.visible=false;
    good(last->set_options(hidden));(void)take(tree->drawing_snapshot({480,120}));
    require(take(first->root().snapshot()).bounds.width>full_width,"Hidden child did not release its share of TabBar width");
    hidden.visible=true;good(last->set_options(hidden));
    good(first->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(last->root().snapshot()).focused&&take(bar->options()).current_index==0&&selections.empty(),
            "TabBar focus movement did not skip disabled tabs or changed selection");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    require(take(bar->options()).current_index==2&&take(last->root().snapshot()).selected&&selections==std::vector<int>({2})&&clicks==1,
            "TabBar activation did not select before its child action");
    auto config=take(bar->options());config.current_index=1;require(!bar->set_options(config)&&take(bar->options()).current_index==2,
            "TabBar accepted a disabled selection");
    config.current_index=0;good(bar->set_options(config));fail=true;config.current_index=2;
    require(!bar->set_options(config)&&take(bar->options()).current_index==0&&take(first->root().snapshot()).selected&&!take(last->root().snapshot()).selected,
            "A rejected child did not restore the TabBar selection");
    throw_on_failure=true;
    require(!bar->set_options(config)&&take(bar->options()).current_index==0&&take(first->root().snapshot()).selected&&!take(last->root().snapshot()).selected,
            "A throwing child did not restore the TabBar selection");throw_on_failure=false;
    fail=false;reenter=true;good(last->root().activate());
    require(!reenter&&take(bar->options()).current_index==0&&take(first->root().snapshot()).selected&&clicks==2,
            "A selection callback's newer update was overwritten");
    good(bar->root().set_visible(false));auto hidden_group_tab=take(first->options());hidden_group_tab.configuration.text="Hidden group";
    good(first->set_options(hidden_group_tab));require(take(bar->options()).current_index==0,
            "Hiding an ancestor changed the group's authored selection");good(bar->root().set_visible(true));
    config=take(bar->options());config.auto_select=false;good(bar->set_options(config));
    auto authored=take(last->options());authored.configuration.selected=true;good(last->set_options(authored));
    require(take(last->root().snapshot()).selected,"Manual selection did not restore authored tab state");
    good(last->root().activate());require(take(bar->options()).current_index==0&&clicks==3,"Manual TabBar changed selection on activation");
    bool rejected=false;std::thread worker([&]{auto r=bar->set_options(config);rejected=!r&&r.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread changed a TabBar");auto registration=bar->weak_registration_port();good(host.clear_content());
    require(!bar->set_options(config)&&!registration(first->weak_group_port()),"Retired TabBar accepted changes");
    std::optional<lvui::native::NativeTabBar> empty;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{auto created=lvui::native::NativeTabBar::create(builder,{},"empty");
        if(!created)return std::unexpected(created.error());empty=*created;return empty->finalize();}));
    auto empty_config=lvui::TabBarConfiguration{};empty_config.spacing=8;good(empty->set_options(empty_config));
    require(take(empty->options()).current_index==-1,"Empty TabBar did not retain an empty selection");good(host.clear_content());tree->close();
}
