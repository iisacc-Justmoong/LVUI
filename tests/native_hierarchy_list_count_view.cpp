#include "hierarchy_list_test_binding.h"
#include <LVUI/NativeBindings.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
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
template<class T>T take(lvui::HierarchyResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
}

int main(){
    using namespace ii::gui;
    using Row=lvui::native::HierarchyRowContext;
    std::vector<lvui::HierarchyEntry> entries{{.key="one",.label="One",.count=5},{.key="two",.label="Two",.count=7}};
    for(int n=0;n<30;++n)entries.push_back({.key="row"+std::to_string(n),.label="Row",.count=n});
    auto model=std::make_shared<lvui::HierarchyMemory>(entries);
    auto controller=std::make_shared<lvui::HierarchyController>(model);
    lvui::native::HierarchyListInput input;input.controller=controller;input.width=220;input.height=40;input.overscan=0;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{260,80}));
    std::optional<HierarchyListTestView> list;
    std::map<std::string,std::shared_ptr<const Row>> contexts;
    std::map<std::string,Widget> badges;
    std::shared_ptr<const Row> rejected_context;
    bool reject_bad=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=HierarchyListTestView::create(builder,{},"list",input,{},
#ifdef LVUI_TEST_HIERARCHY_ROW_DELEGATE
            {},
#endif
            [&](WidgetBuilder& content,std::shared_ptr<const Row> row)->ii::core::Result<void>{
                const auto initial=row->get();const auto key=initial.entry.key;
                if(reject_bad&&key=="bad"){rejected_context=row;return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Rejected count View");}
                auto label=content.label({},"badge-"+key,{.text=std::to_string(initial.entry.count)});
                if(!label)return std::unexpected(label.error());
                auto subscription=std::make_shared<ii::core::ObservableSubscription>(row->observe(
                    [label=*label](const auto& next,std::uint64_t){good(label.set_text(std::to_string(next.entry.count)));}));
                if(auto retained=content.retain(subscription);!retained)return retained;
                contexts[key]=std::move(row);badges[key]=*label;return {};
            });
        if(!value)return std::unexpected(value.error());list=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({260,80}));
    (void)take(list->root().snapshot());
    require(contexts.contains("one")&&contexts.contains("two"),"Visible hierarchy count Views were not mounted");
    auto one=contexts.at("one"),two=contexts.at("two");auto badge=badges.at("one");
    require(one->get().entry.count==5&&two->get().entry.count==7&&one->get().source_index==0&&
            take(badge.snapshot()).text=="5","Per-row count Views received the wrong source data");
    const auto identity=take(badge.placement()).scope.lock();
    double observed_height=0;
    auto height_subscription=one->observe([&](const auto& value,std::uint64_t){observed_height=value.height;});
    require(take(controller->set_row_extent("one",32.)),"A mounted row height could not change");
    ++input.view_revision;good(list->set_options(input));
    require(one->get().height==32&&observed_height==32&&take(badge.placement()).scope.lock()==identity&&
            take(model->snapshot())->revision==0,"A row context did not publish its new height without editing the source");
    require(take(controller->set_row_extent("one",std::nullopt)),"A mounted row height could not reset");
    ++input.view_revision;good(list->set_options(input));
    require(one->get().height==20&&observed_height==20,"A row context retained a cleared height");
    lvui::native::Bindings bindings;std::int64_t observed=-1;
    require(bool(bindings.bind([one](lvui::native::ReadContext& context){return context.read(*one).entry.count;},
        [&](std::int64_t value)->ii::core::Result<void>{observed=value;return {};}))&&observed==5,
        "A row context could not be observed by the native binding graph");
    std::uint64_t source_revision=0;
    const auto replace=[&](const std::vector<lvui::HierarchyEntry>& value){
        source_revision=take(model->replace(value,source_revision));input.source_revision=std::int64_t(source_revision);
    };
    entries[0].count=8;replace(entries);good(list->set_options(input));
    require(one==contexts.at("one")&&one->get().entry.count==8&&take(badge.snapshot()).text=="8"&&
            observed==8&&take(badge.placement()).scope.lock()==identity,"A retained row did not update its reactive count View");
    entries[0].count=-1;replace(entries);good(list->set_options(input));
#ifdef LVUI_TEST_HIERARCHY_ROW_DELEGATE
    require(take(badge.interaction()).visible&&one->get().entry.count==-1,"A negative count hid a custom row View");
#else
    require(!take(badge.interaction()).visible&&one->get().entry.count==-1,"A missing count left the custom View visible");
#endif
    entries[0].count=8;replace(entries);good(list->set_options(input));
    require(take(badge.interaction()).visible&&take(badge.placement()).scope.lock()==identity,"Restoring a count recreated its View");
    entries[0].enabled=false;replace(entries);good(list->set_options(input));
    require(!one->get().enabled&&!take(badge.interaction()).enabled,"A disabled row left its count View enabled");
    entries[0].enabled=true;replace(entries);good(list->set_options(input));
    std::swap(entries[0],entries[1]);input.overscan=1;replace(entries);good(list->set_options(input));
    good(list->root().set_scroll_offset(0));
    require(one->get().source_index==1&&two->get().source_index==0&&
            take(badge.placement()).scope.lock()==identity,"Row reordering lost a retained context or its source index");
    bool reentered=false;
    auto reentrant=one->observe([&](const auto& value,std::uint64_t){
        if(value.entry.count==10&&!reentered){reentered=true;entries[1].count=11;replace(entries);good(list->set_options(input));}
    });
    entries[1].count=10;replace(entries);good(list->set_options(input));
    require(reentered&&one->get().entry.count==11&&observed==11&&take(badge.snapshot()).text=="11",
            "A reentrant row update was replaced by an older publication");
    good(take(list->row("one")).row().activate());
    require(one->get().selected&&!two->get().selected,"Selection was not published to the matching row context");
    bool foreign_rejected=false;std::thread worker([&]{try{(void)one->get();}catch(const std::logic_error&){foreign_rejected=true;}});worker.join();
    require(foreign_rejected,"A row context was read from a foreign thread");
    auto bad=entries;bad[0]={.key="bad",.label="Bad",.count=1};reject_bad=true;replace(bad);
    require(!list->set_options(input)&&one->get().entry.count==11&&take(badge.snapshot()).text=="11"&&list->row("two"),
            "A rejected count View replaced the previously published rows");
    require(bool(rejected_context),"The rejecting factory was not invoked");
    bool candidate_closed=false;try{(void)rejected_context->get();}catch(const std::logic_error&){candidate_closed=true;}
    require(candidate_closed,"A failed candidate retained an open row context");
    reject_bad=false;replace(entries);good(list->set_options(input));
    good(list->root().set_scroll_offset(400));
    bool closed=false;try{(void)one->get();}catch(const std::logic_error&){closed=true;}
    require(closed&&!badge.snapshot(),"A retired row kept its count View or reactive context alive");
    good(bindings.close());good(host.clear_content());tree->close();

    std::vector<lvui::HierarchyEntry> replacement{{.key="first",.label="First",.count=1},
                                                   {.key="second",.label="Second",.count=2}};
    auto next_model=std::make_shared<lvui::HierarchyMemory>(replacement);
    auto next_controller=std::make_shared<lvui::HierarchyController>(next_model);
    lvui::native::HierarchyListInput next_input;next_input.controller=next_controller;next_input.width=220;next_input.height=40;
    auto next_tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto next_host=take(next_tree->content_host({},"next-host",{260,80}));
    std::optional<HierarchyListTestView> next_list;std::shared_ptr<const Row> first_context;bool refreshed=false;
    good(next_host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=HierarchyListTestView::create(builder,{},"next-list",next_input,{},
#ifdef LVUI_TEST_HIERARCHY_ROW_DELEGATE
            {},
#endif
            [&](WidgetBuilder& content,std::shared_ptr<const Row> context)->ii::core::Result<void>{
                if(context->get().entry.key=="first"){
                    first_context=context;
                    if(!refreshed){refreshed=true;replacement[0].count=9;
                        next_input.source_revision=std::int64_t(take(next_model->replace(replacement,0)));
                        good(next_list->set_options(next_input));}
                }
                auto label=content.label({},"count",{.text=std::to_string(context->get().entry.count)});
                return label?ii::core::Result<void>{}:std::unexpected(label.error());
            });
        if(!value)return std::unexpected(value.error());next_list=*value;return {};
    }));
    (void)take(next_tree->drawing_snapshot({260,80}));(void)take(next_list->root().snapshot());
    require(refreshed&&first_context&&first_context->get().entry.count==9&&next_list->row("first")&&next_list->row("second"),
        "A reentrant factory update lost mounted row identities or published stale data");
    good(next_host.clear_content());
    bool parent_closed=false;try{(void)first_context->get();}catch(const std::logic_error&){parent_closed=true;}
    require(parent_closed,"Retiring the list kept its mounted row context open");next_tree->close();
}
