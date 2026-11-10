#include "native_tab_composition.h"
#include <iiGUI/WidgetState.h>
#include <cmath>
#include <map>
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
}

int main(){
    using namespace ii::gui;using namespace lvui::native;
    using lvui::native::NativeTab;using lvui::native::NativeTabBar;using lvui::native::TabInput;using lvui::native::TabGroupPort;using lvui::native::TabSectionPort;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto outer=take(tree->content_host({},"outer",{600,120}));
    std::optional<NativeTabBar> bar;std::optional<NativeTab> first,last;
    TabSectionPort section;Widget slot;std::vector<int> changes;bool throw_changes=false;
    TabInput input;input.configuration.icon_name="home";
    good(outer.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto root=take(builder.vstack({},"root"));
        bar=take(NativeTabBar::create(builder,root,"tabs",{},[&](int value){if(throw_changes)throw std::runtime_error("selection callback failed");changes.push_back(value);}));
        input.configuration.text="First";first=take(NativeTab::create(builder,bar->root(),"first",input));
        if(auto r=bar->register_tab(first->weak_group_port());!r)return r;
        slot=take(builder.content_host(bar->root(),"slot",{}));
        auto created=bar->register_section(slot);if(!created)return std::unexpected(created.error());section=*created;
        input.configuration.text="Last";last=take(NativeTab::create(builder,bar->root(),"last",input));
        if(auto r=bar->register_tab(last->weak_group_port());!r)return r;
        return bar->finalize();
    }));
    good(bar->root().set_extent({600,40}));
    good(last->root().activate());changes.clear();
    std::optional<NativeTab> a,b;Widget group;
    good(slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        group=take(builder.hstack({},"group"));good(group.set_collapse_when_empty(true));
        auto value=input;value.configuration.text="A";a=take(NativeTab::create(builder,group,"a",value));
        value.configuration.text="B";b=take(NativeTab::create(builder,group,"b",value));return {};
    },[&]()->ii::core::Result<void>{
        auto ap=a->weak_group_port(),bp=b->weak_group_port();ap.layout_groups={group};bp.layout_groups={group};return section({ap,bp});
    }));
    (void)take(tree->drawing_snapshot({600,120}));
    require(take(bar->options()).current_index==3&&take(last->root().snapshot()).selected&&changes==std::vector<int>{3},
        "Inserting tabs did not preserve selection identity and report its new index");
    auto ar=take(a->root().snapshot()).bounds,br=take(b->root().snapshot()).bounds;
    require(std::abs(ar.width-br.width)<.01&&std::abs(br.x-ar.x-ar.width-4)<.01,
        "Grouped tabs did not inherit their parent's equal widths and spacing");
    auto configuration=take(bar->options());configuration.spacing=6;good(bar->set_options(configuration));
    (void)take(tree->drawing_snapshot({600,120}));ar=take(a->root().snapshot()).bounds;br=take(b->root().snapshot()).bounds;
    require(std::abs(br.x-ar.x-ar.width-6)<.01,"Grouped tabs did not follow a spacing update");
    good(first->root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(a->root().snapshot()).focused,"Keyboard navigation skipped a conditional tab");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(b->root().snapshot()).focused,"Keyboard navigation missed a nested tab");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(last->root().snapshot()).focused,"Keyboard navigation did not leave the dynamic section");
    auto hidden_a=take(a->options()),hidden_b=take(b->options());hidden_a.visible=false;hidden_b.visible=false;
    good(a->set_options(hidden_a));good(b->set_options(hidden_b));(void)take(tree->drawing_snapshot({600,120}));
    const auto first_bounds=take(first->root().snapshot()).bounds,last_bounds=take(last->root().snapshot()).bounds;
    require(std::abs(last_bounds.x-first_bounds.x-first_bounds.width-6)<.01,"A fully hidden section left a layout gap");
    hidden_a.visible=true;hidden_b.visible=true;good(a->set_options(hidden_a));good(b->set_options(hidden_b));
    require(!section({first->weak_group_port()}),"A tab outside the section was accepted");
    auto duplicate=a->weak_group_port();duplicate.layout_groups={group};
    require(!section({duplicate,duplicate})&&take(last->root().snapshot()).selected,
        "A duplicate candidate altered the published section");
    auto missing=a->weak_group_port();require(!section({missing}),"A retained tab changed layout ancestry");
    std::optional<NativeTab> candidate;bool detached=false;
    auto failed=slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        candidate=take(NativeTab::create(builder,{},"rejected",input));return {};
    },[&]()->ii::core::Result<void>{
        auto port=candidate->weak_group_port();
        port.changed=[](auto)->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::failed,"reject observer");};
        auto result=section({port});auto probe=candidate->weak_group_port();detached=bool(probe.activated([]{return true;}));
        if(detached)(void)probe.activated({});return result;
    });
    require(!failed&&detached&&take(a->root().snapshot()).visible&&take(last->root().snapshot()).selected,
        "A rejected observer left an activation hook or changed the published tabs");
    failed=slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        candidate=take(NativeTab::create(builder,{},"presentation",input));return {};
    },[&]()->ii::core::Result<void>{
        auto port=candidate->weak_group_port();port.present=[](auto)->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::failed,"reject presentation");};
        return section({port});
    });
    require(!failed&&take(a->root().snapshot()).visible&&take(last->root().snapshot()).selected&&take(bar->options()).current_index==3,
        "A rejected tab presentation changed the published section");
    throw_changes=true;
    failed=slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        candidate=take(NativeTab::create(builder,{},"throwing-callback",input));return {};
    },[&]()->ii::core::Result<void>{return section({candidate->weak_group_port()});});
    throw_changes=false;
    require(!failed&&take(a->root().snapshot()).visible&&take(last->root().snapshot()).selected&&take(bar->options()).current_index==3,
        "A throwing selection callback left the candidate section published");
    failed=slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        candidate=take(NativeTab::create(builder,{},"reentrant",input));return {};
    },[&]()->ii::core::Result<void>{
        auto port=candidate->weak_group_port();auto activate=port.activated;
        port.activated=[&,activate](auto hook)->ii::core::Result<void>{
            if(hook){auto latest=take(bar->options());latest.spacing=7;good(bar->set_options(latest));}
            return activate(std::move(hook));};
        return section({port});
    });
    require(!failed&&take(bar->options()).spacing==7&&take(a->root().snapshot()).visible&&take(last->root().snapshot()).selected,
        "A reentrant attachment overwrote newer options or the committed section");
    configuration=take(bar->options());configuration.spacing=6;good(bar->set_options(configuration));
    bool foreign_thread_rejected=false;std::thread worker([&]{auto r=section({});foreign_thread_rejected=!r&&r.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(foreign_thread_rejected,"A foreign thread replaced a dynamic TabBar section");

    std::map<std::string,NativeTab> rows;
    const auto row=[&](std::string key)->KeyedContentItem{return {key,[&,key](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=input;value.configuration.text=key;auto item=NativeTab::create(builder,{},"tab",value);
        if(!item)return std::unexpected(item.error());rows.insert_or_assign(key,*item);return {};
    }};};
    const auto publish=[&](std::vector<std::string> keys)->ii::core::Result<void>{
        std::vector<TabGroupPort> ports;for(const auto& key:keys)ports.push_back(rows.at(key).weak_group_port());return section(std::move(ports));
    };
    const KeyedContentOptions layout{.horizontal=true,.layout={.spacing=6,.alignment=StackAlignment::center}};
    good(slot.set_keyed_content({row("x"),row("y")},layout,[&]{return publish({"x","y"});}));
    auto x=rows.at("x").root(),y=rows.at("y").root();const auto identity=take(x.snapshot()).id;
    good(x.activate());changes.clear();
    good(slot.set_keyed_content({row("y"),row("x")},layout,[&]{return publish({"y","x"});}));
    require(take(x.snapshot()).id==identity&&take(x.snapshot()).selected&&take(bar->options()).current_index==2&&changes==std::vector<int>{2},
        "Reordering keyed tabs lost selection identity or failed to report its new index");
    auto rejected=slot.set_keyed_content({row("x"),row("y")},layout,[&]()->ii::core::Result<void>{
        auto port=rows.at("y").weak_group_port();port.present=[](auto)->ii::core::Result<void>{return ii::core::fail(ii::core::ErrorCode::failed,"reject presentation");};
        return section({rows.at("x").weak_group_port(),port});
    });
    // Retained adapters remain owned by the published generation; changing the
    // candidate port alone must not replace them.
    require(rejected&&take(x.snapshot()).selected,"Retained tab adapters were replaced on reorder");
    good(slot.set_content({},[&]{return section({});}));
    require(!x.snapshot()&&!y.snapshot()&&take(bar->options()).current_index==0&&take(first->root().snapshot()).selected,
        "Retiring the selected dynamic tab did not choose the first available tab");
    auto stale=section;good(outer.clear_content());require(!stale({}),"A retired tab section accepted changes");
    std::optional<NativeTabBar> empty_bar;std::optional<NativeTab> late;TabSectionPort empty_section;Widget empty_slot;
    good(outer.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        empty_bar=take(NativeTabBar::create(builder,{},"empty"));empty_slot=take(builder.content_host(empty_bar->root(),"empty-slot",{}));
        empty_section=take(empty_bar->register_section(empty_slot));return empty_bar->finalize();
    }));
    require(take(empty_bar->options()).current_index==-1,"An empty TabBar has a selection");
    good(empty_slot.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        late=take(NativeTab::create(builder,{},"late",input));return {};
    },[&]()->ii::core::Result<void>{return empty_section({late->weak_group_port()});}));
    require(take(empty_bar->options()).current_index==0&&take(late->root().snapshot()).selected,
        "The first dynamically inserted tab was not automatically selected");
    good(outer.clear_content());tree->close();
}
