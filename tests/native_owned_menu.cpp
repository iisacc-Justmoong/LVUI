#include "native_menu_composition.h"
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <cmath>
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
void retired_menu_releases_callbacks(){
    using namespace ii::gui;using namespace lvui::native;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,160}));std::optional<NativeMenu> retained;
    auto payload=std::make_shared<int>(7);std::weak_ptr<int> weak=payload;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        NativeMenuCallbacks callbacks;callbacks.opened=[payload]{};
        auto result=NativeMenu::create(builder,{},"menu",{},std::move(callbacks));
        if(!result)return std::unexpected(result.error());retained=*result;return retained->finalize();
    }));
    payload.reset();require(!weak.expired(),"Live menu released its callback");
    good(host.clear_content());require(weak.expired(),"Retired menu retained callback through its public handle");
    tree->close();
}
void retired_menu_releases_item_ports(bool retire_during_update){
    using namespace ii::gui;using namespace lvui::native;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{320,160}));std::optional<NativeMenu> retained;std::optional<NativeMenuItem> item;
    auto payload=std::make_shared<int>(7);std::weak_ptr<int> weak=payload;bool retire=false;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto menu=NativeMenu::create(builder,{},"menu");if(!menu)return std::unexpected(menu.error());retained=*menu;
        MenuItemInput input;input.configuration.icon_name.clear();input.configuration.show_icon_slot=false;input.configuration.show_chevron=false;
        auto created=NativeMenuItem::create(builder,menu->content_root(),"item",input);if(!created)return std::unexpected(created.error());item=*created;
        auto port=item->weak_group_port();auto present=port.present;
        port.present=[present,payload,&retire,&host](auto value)->ii::core::Result<void>{
            if(retire){retire=false;good(host.clear_content());}return present(value);};
        if(auto attached=menu->register_item(std::move(port));!attached)return attached;return menu->finalize();
    }));
    payload.reset();require(!weak.expired(),"Live menu released its item port");
    if(retire_during_update){auto value=take(retained->options());value.configuration.item_width+=1;retire=true;
        auto changed=retained->set_options(value);require(!changed,"Retired menu published an update");}
    else good(host.clear_content());
    require(weak.expired(),"Retired menu retained item port resources");tree->close();
}
void hidden_chevron_leaf(){
    using namespace ii::gui;using namespace lvui::native;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{400,240}));std::optional<NativeMenu> menu;
    std::optional<NativeMenuItem> item;int calls=0;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"root");if(!stack)return std::unexpected(stack.error());
        auto created=NativeMenu::create(builder,*stack,"menu");if(!created)return std::unexpected(created.error());menu=*created;
        MenuItemInput input;input.configuration.label="Action";input.configuration.show_icon_slot=false;
        input.configuration.show_chevron=false; // has_children retains the inherited default.
        auto leaf=NativeMenuItem::create(builder,menu->content_root(),"leaf",input,{.clicked=[&]{++calls;}});
        if(!leaf)return std::unexpected(leaf.error());item=*leaf;
        if(auto result=menu->register_item(item->weak_group_port());!result)return result;return menu->finalize();
    }));
    good(menu->root().open_at(20,20));good(item->root().activate());
    require(calls==1&&!take(menu->root().snapshot()).opened,
            "A leaf with a hidden chevron did not invoke its action and close the menu");
    good(host.clear_content());tree->close();
}
void legacy_popup_adapter(){
    using namespace ii::gui;using namespace lvui::native;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));
    std::optional<NativeMenu> parent,child;std::optional<NativeMenuItem> owner,other;
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"root");if(!stack)return std::unexpected(stack.error());
        auto menu=NativeMenu::create(builder,*stack,"parent");if(!menu)return std::unexpected(menu.error());parent=*menu;
        MenuItemInput input;input.configuration.label="Owner";input.configuration.show_icon_slot=false;input.configuration.show_chevron=false;
        auto first=NativeMenuItem::create(builder,parent->content_root(),"owner",input);if(!first)return std::unexpected(first.error());owner=*first;
        if(auto r=parent->register_item(owner->weak_group_port());!r)return r;
        auto popup=NativeMenu::create(builder,owner->root(),"child");if(!popup)return std::unexpected(popup.error());child=*popup;
        if(auto r=child->finalize();!r)return r;
        auto legacy=child->weak_popup_port();legacy.close={};
        if(auto r=owner->set_submenu(std::move(legacy));!r)return r;
        input.configuration.label="Other";input.configuration.has_children=false;
        auto second=NativeMenuItem::create(builder,parent->content_root(),"other",input);if(!second)return std::unexpected(second.error());other=*second;
        if(auto r=parent->register_item(other->weak_group_port());!r)return r;return parent->finalize();
    }));
    good(parent->root().open_at(20,20));good(owner->root().activate());
    require(take(child->root().snapshot()).opened,"A legacy popup adapter did not open normally");
    const auto bounds=take(other->root().snapshot()).bounds;
    auto moved=tree->input({.kind=WidgetInputKind::pointer_move,.x=bounds.x+bounds.width/2,.y=bounds.y+bounds.height/2});
    require(!moved&&moved.error().code==ii::core::ErrorCode::unsupported&&take(child->root().snapshot()).opened,
        "A legacy popup without close support was silently discarded during hover");
    good(parent->root().close_popup());good(host.clear_content());tree->close();
}
}
int main(){
    retired_menu_releases_callbacks();
    retired_menu_releases_item_ports(false);
    retired_menu_releases_item_ports(true);
    using namespace ii::gui;auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,300}));std::optional<lvui::native::NativeMenu> menu;
    std::optional<lvui::native::NativeMenuItem> first,disabled,submenu;Widget anchor;std::vector<std::string> events;bool reopen=false;
    lvui::native::MenuInput input;input.configuration.accessibility_name="Actions";
    good(host.set_content([&](auto& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto target=builder.action_surface(*stack,"anchor",{.text="Open",.width=80,.height=25});if(!target)return std::unexpected(target.error());anchor=*target;
        auto created=lvui::native::NativeMenu::create(builder,*stack,"menu",input,
            {.triggered=[&](int index){events.push_back("trigger:"+std::to_string(index));},.opened=[&]{events.push_back("open");},
             .closed=[&]{events.push_back("close");},.canceled=[&]{events.push_back("cancel");}});
        if(!created)return std::unexpected(created.error());menu=*created;
        lvui::native::MenuItemInput value;value.configuration.label="First";value.configuration.has_children=false;value.configuration.height=31;
        auto a=lvui::native::NativeMenuItem::create(builder,menu->content_root(),"first",value,{.clicked=[&]{events.push_back("item");
            if(reopen){reopen=false;good(menu->root().close_popup());good(menu->root().open_at(50,50));}}});
        if(!a)return std::unexpected(a.error());first=*a;if(auto r=menu->register_item(first->weak_group_port());!r)return r;
        value.configuration.label="Disabled";value.configuration.enabled=false;value.configuration.height=0;
        auto b=lvui::native::NativeMenuItem::create(builder,menu->content_root(),"disabled",value);if(!b)return std::unexpected(b.error());disabled=*b;
        if(auto r=menu->register_item(disabled->weak_group_port());!r)return r;
        value.configuration.label="Submenu";value.configuration.enabled=true;value.configuration.has_children=true;
        auto c=lvui::native::NativeMenuItem::create(builder,menu->content_root(),"submenu",value,{.clicked=[&]{events.push_back("submenu");}});
        if(!c)return std::unexpected(c.error());submenu=*c;if(auto r=menu->register_item(submenu->weak_group_port());!r)return r;
        if(auto r=anchor.set_context_menu(menu->root());!r)return r;return menu->finalize();
    }));
    auto semantics=take(menu->content_root().snapshot());require(semantics.accessibility_semantics&&semantics.accessibility_semantics->role==AccessibilityRole::menu&&
        semantics.accessibility_name=="Actions","Menu accessibility group was not published");
    good(menu->root().open_at(20,20));require(take(menu->root().snapshot()).opened&&events==std::vector<std::string>({"open"}),"Menu did not open");
    require(take(menu->root().snapshot()).bounds.height==87&&take(first->root().snapshot()).bounds.height==31,
        "The menu did not account for a custom item height");
    const auto bounds=take(menu->content_root().snapshot()).bounds;const auto frame=take(tree->drawing_snapshot({500,300}));
    const auto surface=[&](float alpha){return std::ranges::any_of(frame.overlays,[&](const auto& overlay){return std::ranges::any_of(overlay.panels,[&](const auto& panel){
        return std::abs(panel.bounds.x-bounds.x)<.01&&std::abs(panel.bounds.y-bounds.y)<.01&&
            std::abs(panel.bounds.width-bounds.width)<.01&&std::abs(panel.bounds.height-bounds.height)<.01&&
            panel.stroke_width==0&&std::abs(panel.top.alpha-alpha)<.001;});});};
    require(surface(1)&&surface(.12f),"Menu coating did not preserve an opaque base and the inherited coating opacity");
    auto before=take(first->options());auto oversized=before;oversized.configuration.height=8192;
    require(!first->set_options(oversized)&&take(first->options())==before&&take(menu->root().snapshot()).bounds.height==87&&bool(menu->status()),
        "An oversized item changed its menu or reported success");
    good(first->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(submenu->root().snapshot()).focused&&take(menu->selected_index())==2,"Menu keyboard navigation did not skip the disabled item");
    good(submenu->root().activate());require(take(menu->root().snapshot()).opened&&events==std::vector<std::string>({"open","submenu","trigger:2"}),
        "Submenu activation incorrectly closed its parent or changed callback order");
    events.clear();good(first->root().activate());require(!take(menu->root().snapshot()).opened&&events==std::vector<std::string>({"item","trigger:0","close"}),
        "Menu action did not dispatch callbacks before closing");
    events.clear();good(menu->root().open_at(20,20));reopen=true;good(first->root().activate());
    require(!reopen&&take(menu->root().snapshot()).opened&&events==std::vector<std::string>({"open","item","close","open","trigger:0"}),
        "An old activation closed a reopened menu");
    good(menu->root().close_popup());events.clear();auto target=take(anchor.snapshot());
    good(tree->input({.kind=WidgetInputKind::context_request,.x=target.bounds.x+4,.y=target.bounds.y+4,.target=target.id}));
    require(take(menu->root().snapshot()).opened&&events==std::vector<std::string>({"open"}),"Generic context request did not open the independent menu");
    auto invalid=input;invalid.selected_index=1;require(!menu->set_options(invalid)&&take(menu->options()).selected_index==-1,
        "Disabled selection changed menu options");
    auto changed=take(disabled->options());changed.configuration.enabled=true;good(disabled->set_options(changed));
    input.selected_index=1;good(menu->set_options(input));require(take(menu->selected_index())==1&&take(disabled->root().snapshot()).selected,
        "Reactive menu selection did not reach its item");
    changed.visible=false;good(disabled->set_options(changed));require(take(menu->selected_index())==-1,"Hiding selected menu item retained a stale selection");
    auto access=menu->weak_section_access();require(take(access.options())==take(menu->options()),"Menu section access lost its options");
    bool rejected=false;std::thread worker([&]{auto r=menu->set_options(input);auto o=access.options();auto s=access.register_section(anchor);
        rejected=!r&&r.error().code==ii::core::ErrorCode::wrong_executor&&!o&&o.error().code==ii::core::ErrorCode::wrong_executor&&
            !s&&s.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(rejected,"Foreign thread changed or inspected a menu");auto registration=menu->weak_registration_port();good(host.clear_content());
    require(!menu->set_options(input)&&!registration(first->weak_group_port())&&!access.options()&&!access.register_section(anchor),
        "Retired menu accepted changes or section access");tree->close();
    legacy_popup_adapter();
    hidden_chevron_leaf();
}
