#include "native_menu_composition.h"
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}
int main(){
    using namespace ii::gui;using namespace lvui::native;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{500,300}));
    std::optional<NativeMenu> menu;std::optional<NativeMenuItem> first,last,conditional;
    MenuSectionPort section;Widget slot;std::vector<int> triggered;
    MenuItemInput input;input.configuration.has_children=false;input.configuration.show_icon_slot=false;
    good(outer.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=take(builder.vstack({},"root"));
        menu=take(NativeMenu::create(builder,root,"menu",{}, {.triggered=[&](int n){triggered.push_back(n);}}));
        input.configuration.label="First";first=take(NativeMenuItem::create(builder,menu->content_root(),"first",input));
        if(auto r=menu->register_item(first->weak_group_port());!r)return r;
        slot=take(builder.content_host(menu->content_root(),"slot",{}));
        auto created=menu->register_section(slot);if(!created)return std::unexpected(created.error());section=*created;
        input.configuration.label="Last";last=take(NativeMenuItem::create(builder,menu->content_root(),"last",input));
        if(auto r=menu->register_item(last->weak_group_port());!r)return r;return menu->finalize();
    }));
    good(menu->root().open_at(20,20));const auto empty_height=take(menu->root().snapshot()).bounds.height;
    {
        Widget group;std::optional<NativeMenuItem> a,b;
        good(slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
            group=take(builder.vstack({},"group"));good(group.set_collapse_when_empty(true));
            auto first_input=input;first_input.configuration.label="Group A";
            auto second_input=input;second_input.configuration.label="Group B";
            a=take(NativeMenuItem::create(builder,group,"group-a",first_input));
            b=take(NativeMenuItem::create(builder,group,"group-b",second_input));return {};
        },[&]()->ii::core::Result<void>{
            auto first_port=a->weak_group_port(),second_port=b->weak_group_port();
            first_port.layout_groups.push_back(group);second_port.layout_groups.push_back(group);
            return section({first_port,second_port});
        }));
        const auto spacing=[&]{const auto first_rect=take(a->root().snapshot()).bounds,second_rect=take(b->root().snapshot()).bounds;
            return double(second_rect.y-first_rect.y-first_rect.height);};
        require(std::abs(spacing()-2)<.01,"A staged menu group did not inherit its parent's spacing");
        require(take(group.stack_layout()).stack.alignment==StackAlignment::stretch,
            "A menu group did not adopt the menu's cross-axis layout");
        auto options=take(menu->options());options.configuration.item_spacing=5;good(menu->set_options(options));
        require(std::abs(spacing()-5)<.01,"A committed menu group did not follow a spacing update");
        auto invalid=a->weak_group_port();invalid.layout_groups.push_back(menu->content_root());
        require(!section({invalid})&&std::abs(spacing()-5)<.01&&take(b->root().snapshot()).visible,
            "A group outside the section changed the committed menu");
        auto duplicate=a->weak_group_port();duplicate.layout_groups={group,group};
        auto missing=a->weak_group_port();
        require(!section({duplicate})&&!section({missing})&&std::abs(spacing()-5)<.01&&take(b->root().snapshot()).visible,
            "Duplicate or changed layout groups altered a retained menu item");
        Widget sized_group;std::optional<NativeMenuItem> sized_item;
        auto sized=slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
            sized_group=take(builder.vstack({},"sized-group"));good(sized_group.set_extent({140,25}));
            sized_item=take(NativeMenuItem::create(builder,sized_group,"sized-item",input));return {};
        },[&]()->ii::core::Result<void>{auto port=sized_item->weak_group_port();port.layout_groups.push_back(sized_group);return section({port});});
        require(!sized&&std::abs(spacing()-5)<.01&&take(b->root().snapshot()).visible,
            "A group with a fixed extent changed the committed menu");
        Widget reentrant_group;std::optional<NativeMenuItem> reentrant_item;bool observed=false;
        const auto opacity=take(menu->options()).configuration.opacity;
        auto reentrant=slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
            reentrant_group=take(builder.vstack({},"reentrant-group"));
            reentrant_item=take(NativeMenuItem::create(builder,reentrant_group,"reentrant-item",input));return {};
        },[&]()->ii::core::Result<void>{
            good(reentrant_group.set_layout_handler([&](Rect){if(observed)return;observed=true;
                auto changed=take(menu->options());changed.configuration.opacity=.37;good(menu->set_options(changed));}));
            auto port=reentrant_item->weak_group_port();port.layout_groups.push_back(reentrant_group);return section({port});
        });
        require(!reentrant&&observed&&take(menu->options()).configuration.opacity==.37&&
                std::abs(spacing()-5)<.01&&take(b->root().snapshot()).visible,
            "Reentrant group inspection overwrote a newer menu or replaced committed entries");
        auto restore=take(menu->options());restore.configuration.opacity=opacity;good(menu->set_options(restore));
        options.configuration.item_spacing=8192;
        require(!menu->set_options(options)&&std::abs(spacing()-5)<.01,
            "A rejected menu spacing changed the grouped layout");
        auto first_input=take(a->options()),second_input=take(b->options());first_input.visible=false;second_input.visible=false;
        good(a->set_options(first_input));good(b->set_options(second_input));
        require(take(menu->root().snapshot()).bounds.height==empty_height+3,"A hidden group left a layout gap");
        options.configuration.item_spacing=2;good(menu->set_options(options));good(slot.clear_content());
        require(take(menu->root().snapshot()).bounds.height==empty_height,"A retired group retained menu geometry");
    }
    {
        Widget outer_group,first_group,second_group;std::vector<NativeMenuItem> items;
        good(slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
            outer_group=take(builder.vstack({},"outer-group"));first_group=take(builder.vstack(outer_group,"first-group"));
            second_group=take(builder.vstack(outer_group,"second-group"));
            for(int i=0;i<4;++i){auto value=input;value.configuration.label="Nested "+std::to_string(i);
                items.push_back(take(NativeMenuItem::create(builder,i<2?first_group:second_group,"nested-"+std::to_string(i),value)));}
            return {};
        },[&]()->ii::core::Result<void>{
            std::vector<MenuItemPort> ports;
            for(std::size_t i=0;i<items.size();++i){auto value=items[i].weak_group_port();
                value.layout_groups={outer_group,i<2?first_group:second_group};ports.push_back(std::move(value));}
            return section(std::move(ports));
        }));
        int inconsistent_layouts=0;
        good(first_group.set_layout_handler([&](Rect){
            const auto a=take(first_group.stack_layout()),b=take(second_group.stack_layout());
            if(a.stack.spacing!=b.stack.spacing)++inconsistent_layouts;
        }));
        (void)take(first_group.snapshot());inconsistent_layouts=0;
        auto options=take(menu->options());options.configuration.item_spacing=5;good(menu->set_options(options));
        (void)take(menu->root().snapshot());
        require(inconsistent_layouts==0&&take(first_group.stack_layout()).stack.spacing==5&&take(second_group.stack_layout()).stack.spacing==5,
            "A spacing update exposed a partially updated grouped layout");
        options.configuration.item_spacing=2;good(menu->set_options(options));good(slot.clear_content());
    }
    good(slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=input;value.configuration.label="Conditional";
        auto item=NativeMenuItem::create(builder,{},"conditional",value);if(!item)return std::unexpected(item.error());conditional=*item;return {};
    },[&]()->ii::core::Result<void>{return section({conditional->weak_group_port()});}));
    require(take(menu->root().snapshot()).bounds.height==empty_height+20,"Conditional item did not update menu geometry");
    const auto conditional_bounds=take(conditional->root().snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=conditional_bounds.x+4,.y=conditional_bounds.y+4}));
    require(take(menu->selected_index())==1&&triggered.empty(),"Pointer navigation missed or activated a dynamic menu item");
    good(first->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(conditional->root().snapshot()).focused&&take(menu->selected_index())==1,
        "Keyboard navigation did not include the conditional item in declaration order");
    good(conditional->root().activate());require(triggered==std::vector<int>{1}&&!take(menu->root().snapshot()).opened,
        "Conditional action did not dispatch and close");
    good(menu->root().open_at(20,20));
    const auto prior=conditional->root();
    good(slot.clear_content());
    require(!prior.snapshot()&&take(menu->root().snapshot()).bounds.height==empty_height&&take(menu->selected_index())==-1,
        "Retiring a conditional item left a selection, gap or live widget");
    std::optional<ii::gui::NativeDivider> divider;
    good(slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        divider=take(ii::gui::NativeDivider::create(builder,{},"divider",ii::gui::DividerConfiguration{}));return {};
    },[&]()->ii::core::Result<void>{auto port=NativeMenu::divider_port(divider->weak_group_port());if(!port)return std::unexpected(port.error());return section({*port});}));
    require(take(menu->root().snapshot()).bounds.height==empty_height+5,"A dynamic divider did not update menu geometry");
    good(first->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(last->root().snapshot()).focused&&take(menu->selected_index())==2,"Keyboard navigation did not skip the dynamic divider");
    good(divider->set_visible(false));require(take(menu->root().snapshot()).bounds.height==empty_height,
        "A hidden dynamic divider left a menu gap");
    good(divider->set_visible(true));good(slot.clear_content());
    require(take(menu->root().snapshot()).bounds.height==empty_height&&take(menu->selected_index())==1,
        "A retired divider left stale menu geometry or moved the selected item");

    std::map<std::string,NativeMenuItem> rows;
    const auto row=[&](std::string key,double height=0)->KeyedContentItem{
        return {key,[&,key,height](WidgetBuilder& builder)->ii::core::Result<void>{
            auto value=input;value.configuration.label=key;value.configuration.height=height;
            auto item=NativeMenuItem::create(builder,{},"row",value);if(!item)return std::unexpected(item.error());
            rows.insert_or_assign(key,*item);return {};
        }};
    };
    const auto commit=[&](std::vector<std::string> keys)->ii::core::Result<void>{
        std::vector<MenuItemPort> ports;for(const auto& key:keys)ports.push_back(rows.at(key).weak_group_port());
        return section(std::move(ports));
    };
    const KeyedContentOptions layout{.layout={.spacing=2,.alignment=StackAlignment::stretch}};
    good(slot.set_keyed_content({row("a"),row("b")},layout,[&]{return commit({"a","b"});}));
    auto a=rows.at("a").root(),b=rows.at("b").root();const auto a_id=take(a.snapshot()).id;
    auto options=take(menu->options());options.selected_index=1;good(menu->set_options(options));
    good(slot.set_keyed_content({row("b"),row("a")},layout,[&]{return commit({"b","a"});}));
    require(take(a.snapshot()).id==a_id&&take(menu->selected_index())==2&&take(a.snapshot()).selected,
        "Reordering lost the selected keyed item or changed its identity");
    auto change=take(rows.at("b").options());change.visible=false;good(rows.at("b").set_options(change));
    require(take(menu->root().snapshot()).bounds.height==empty_height+20,
        "A hidden keyed item left a menu gap");
    change.visible=true;good(rows.at("b").set_options(change));
    const auto height=take(menu->root().snapshot()).bounds.height;
    auto failed=slot.set_keyed_content({row("a"),row("oversized",8192)},layout,[&]{return commit({"a","oversized"});});
    require(!failed&&take(menu->root().snapshot()).bounds.height==height&&take(a.snapshot()).id==a_id&&b.snapshot()&&
            take(slot.keyed_content_keys())==std::vector<std::string>({"b","a"}),
        "An invalid keyed candidate changed the existing menu");
    require(!section({rows.at("a").weak_group_port(),rows.at("a").weak_group_port()})&&
            take(menu->root().snapshot()).bounds.height==height,"Duplicate section entries changed the menu");
    std::optional<NativeMenuItem> rejected;bool activation_detached=false;
    failed=slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=input;value.configuration.label="Rejected";
        rejected=take(NativeMenuItem::create(builder,{},"rejected",value));return {};
    },[&]()->ii::core::Result<void>{
        auto port=rejected->weak_group_port();
        port.changed=[](auto)->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::failed,"reject observer");};
        auto result=section({port});
        auto probe=rejected->weak_group_port();
        activation_detached=bool(probe.activated([](auto){}));
        if(activation_detached)(void)probe.activated({});
        return result;
    });
    require(!failed&&activation_detached&&take(slot.keyed_content_keys())==std::vector<std::string>({"b","a"})&&
            take(menu->root().snapshot()).bounds.height==height,"Failed section attachment retained hooks or changed active rows");
    std::optional<NativeMenuItem> candidate_a,candidate_b;bool owner_detached=false;
    failed=slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=builder.vstack({},"candidate");if(!root)return std::unexpected(root.error());
        candidate_a=take(NativeMenuItem::create(builder,*root,"candidate-a",input));
        candidate_b=take(NativeMenuItem::create(builder,*root,"candidate-b",input));return {};
    },[&]()->ii::core::Result<void>{
        auto a_port=candidate_a->weak_group_port(),b_port=candidate_b->weak_group_port();
        auto changed=a_port.changed;
        a_port.changed=[changed](auto callback)->ii::core::Result<void>{
            if(!callback)throw std::runtime_error("observer detach failed");return changed(std::move(callback));
        };
        b_port.parent_close=[](MenuClosePlan)->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::failed,"reject second owner");};
        auto result=section({a_port,b_port});auto probe=candidate_a->weak_group_port();
        activation_detached=bool(probe.activated([](auto){}));
        owner_detached=bool(probe.parent_close([]()->ii::core::Result<MenuCloseAction>{return MenuCloseAction{[]()->ii::core::Result<void>{return {};}};}));
        if(activation_detached)(void)probe.activated({});if(owner_detached)(void)probe.parent_close({});(void)probe.changed({});
        return result;
    });
    require(!failed&&activation_detached&&owner_detached&&take(slot.keyed_content_keys())==std::vector<std::string>({"b","a"}),
        "One throwing detach skipped the remaining registration cleanup");
    failed=slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=input;value.configuration.label="Presentation failure";
        rejected=take(NativeMenuItem::create(builder,{},"presentation-failure",value));return {};
    },[&]()->ii::core::Result<void>{
        auto port=rejected->weak_group_port();
        port.present=[](MenuItemPresentation)->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::failed,"reject presentation");};
        return section({port});
    });
    require(!failed&&take(slot.keyed_content_keys())==std::vector<std::string>({"b","a"})&&
            take(a.snapshot()).selected&&take(menu->root().snapshot()).bounds.height==height,
        "Failed section presentation did not restore the previous selection and geometry");
    failed=slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=input;value.configuration.label="Reentrant";
        rejected=take(NativeMenuItem::create(builder,{},"reentrant",value));return {};
    },[&]()->ii::core::Result<void>{
        auto port=rejected->weak_group_port();auto parent_close=port.parent_close;
        port.parent_close=[&,parent_close](MenuClosePlan plan)->ii::core::Result<void>{
            if(plan){auto current=take(menu->options());current.configuration.opacity=.33;good(menu->set_options(current));}
            return parent_close(std::move(plan));
        };
        auto result=section({port});auto probe=rejected->weak_group_port();
        activation_detached=bool(probe.activated([](auto){}));if(activation_detached)(void)probe.activated({});return result;
    });
    require(!failed&&activation_detached&&take(menu->options()).configuration.opacity==.33&&
            take(slot.keyed_content_keys())==std::vector<std::string>({"b","a"})&&take(a.snapshot()).selected,
        "A reentrant menu change was overwritten or retained the rejected candidate");
    auto unrelated=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto other=take(unrelated->content_host({},"other",{80,80}));std::optional<NativeMenuItem> foreign;
    good(other.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{foreign=take(NativeMenuItem::create(builder,{},"foreign",input));return {};}));
    require(!section({foreign->weak_group_port()})&&take(menu->root().snapshot()).bounds.height==height,
        "A foreign widget was admitted into the menu section");
    unrelated->close();good(menu->root().close_popup());good(outer.clear_content());
    require(!section({}),"A retired menu section accepted new content");tree->close();
}
