#include "list_view_test_binding.h"
#include <LVUI/NativeBindings.h>
#include <iiGUI/WidgetState.h>
#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
template<class T>T take(lvui::ListResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool text_present(const ii::gui::CanvasFrame& frame,std::string_view text){
    const auto has=[&](const auto& values){return std::ranges::any_of(values,[&](const auto& item){return item.text==text;});};
    if(has(frame.texts))return true;for(const auto& overlay:frame.overlays)if(has(overlay.texts))return true;return false;
}
void row_trigger(){
    using namespace ii::gui;using Row=lvui::native::ListRowContext;
    std::vector<lvui::ListEntry> entries;
    for(int i=0;i<20;++i)entries.push_back({.key=std::to_string(i),.label="Item"});
    auto source=take(lvui::ListMemory::create(std::move(entries)));
    auto controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    lvui::native::ListViewInput input;input.controller=controller;input.width=200;input.height=40;input.overscan=0;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"trigger-host",{200,60}));std::optional<ListViewTestView> list;
    std::map<std::string,std::shared_ptr<const Row>> contexts;int activations=0;bool prepublication_rejected=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto view=ListViewTestView::create(builder,{},"list",input,{.activated=[&](const auto&){++activations;}},
            [&](WidgetBuilder& child,std::shared_ptr<const Row> row)->ii::core::Result<void>{
                const auto key=row->get().entry.key;contexts[key]=row;
                if(key=="0")prepublication_rejected=!row->trigger();
                auto label=child.label({},"label",{.text=key});if(!label)return std::unexpected(label.error());return {};
            });
        if(!view)return std::unexpected(view.error());list=*view;return {};
    }));
    (void)take(tree->drawing_snapshot({200,60}));(void)take(list->root().snapshot());
    require(contexts.contains("0")&&contexts.contains("1"),"The first two row command contexts were not mounted");
    auto first=contexts.at("0"),second=contexts.at("1");
    require(prepublication_rejected&&activations==0,"A candidate row activated before publication");
    good(first->trigger());require(activations==1&&take(controller->selection())=="0"&&first->get().selected,
        "A row command did not select and activate its row");
    auto command=second->get().trigger;require(bool(command),"A row snapshot did not expose its command");
    good(command->request());require(activations==2&&take(controller->selection())=="1"&&second->get().selected,
        "A retained row command did not activate its own row");
    bool wrong=false;std::thread worker([&]{auto result=first->trigger();wrong=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(wrong,"A row command accepted a foreign thread");
    auto changed=take(source->snapshot())->entries[0];changed.label="Newer";
    input.source_revision=std::int64_t(take(source->apply({.revision=0,.kind=lvui::ListEditKind::replace,.key="0",.item=changed})));
    require(take(controller->refresh()),"The shared controller did not observe the newer source");
    good(second->trigger());require(activations==2&&first->get().entry.label=="Newer",
        "A stale row command fired instead of refreshing its source");
    input.interactive=false;good(list->set_options(input));require(!first->trigger()&&activations==2,"A noninteractive list accepted a row command");
    input.interactive=true;good(list->set_options(input));
    auto item=take(source->snapshot())->entries[0];item.enabled=false;
    input.source_revision=std::int64_t(take(source->apply({.revision=1,.kind=lvui::ListEditKind::replace,.key="0",.item=item})));
    good(list->set_options(input));require(!first->trigger()&&activations==2,"A disabled row accepted a command");
    good(list->root().set_scroll_offset(200));require(!first->trigger()&&!command->request(),"An offscreen row command remained live");
    good(list->root().set_scroll_offset(0));require(contexts.at("1")!=second&&!command->request(),
        "An old command targeted a newer row generation with the same key");
    require(take(source->snapshot())->revision==2,"A row command modified the source model");
    good(host.clear_content());require(!contexts.at("1")->trigger(),"A closed list accepted a row command");tree->close();
}
void row_trigger_retires(){
    using namespace ii::gui;using Row=lvui::native::ListRowContext;
    auto source=take(lvui::ListMemory::create({{.key="one",.label="One"}}));
    auto controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    lvui::native::ListViewInput input;input.controller=controller;input.width=200;input.height=40;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"retiring-trigger",{200,60}));std::optional<ListViewTestView> list;
    std::shared_ptr<const Row> context;int activations=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto view=ListViewTestView::create(builder,{},"list",input,{.activated=[&](const auto&){++activations;}},
            [&](WidgetBuilder& child,std::shared_ptr<const Row> row)->ii::core::Result<void>{
                context=std::move(row);auto label=child.label({},"label",{.text="One"});if(!label)return std::unexpected(label.error());return {};
            });
        if(!view)return std::unexpected(view.error());list=*view;return {};
    }));
    (void)take(tree->drawing_snapshot({200,60}));(void)take(list->root().snapshot());
    require(bool(context),"The retiring row was not mounted");bool retired=false;
    auto observer=context->observe([&](const auto& value,std::uint64_t){if(value.selected){retired=true;good(host.clear_content());}});
    good(context->trigger());require(retired&&activations==0&&!context->trigger(),
        "A row command did not safely survive retirement during selection");
    tree->close();
}
void variable_row_height(){
    using namespace ii::gui;using Row=lvui::native::ListRowContext;
    std::vector<lvui::ListEntry> entries;
    for(int i=0;i<20;++i)entries.push_back({.key=std::to_string(i),.label="Item"});
    auto source=take(lvui::ListMemory::create(std::move(entries)));
    auto controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    lvui::native::ListViewInput input;input.controller=controller;input.width=200;input.height=100;input.row_height=20;input.overscan=1;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"variable-host",{200,120}));std::optional<ListViewTestView> list;
    std::map<std::string,std::shared_ptr<const Row>> contexts;std::map<std::string,Widget> roots;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto view=ListViewTestView::create(builder,{},"list",input,{},
            [&](WidgetBuilder& child,std::shared_ptr<const Row> row)->ii::core::Result<void>{
                auto key=row->get().entry.key;contexts[key]=row;
                const double height=key=="0"?60:key=="1"?35:10;
                auto root=child.absolute_stack({},"intrinsic",{200,height});if(!root)return std::unexpected(root.error());roots[key]=*root;return {};
            });
        if(!view)return std::unexpected(view.error());list=*view;return {};
    }));
    (void)take(tree->drawing_snapshot({200,120}));(void)take(list->root().snapshot());
    require(contexts.contains("0")&&contexts.contains("1")&&contexts.contains("2"),"Variable rows were not mounted");
    require(contexts.at("0")->get().height==60&&contexts.at("1")->get().height==35&&contexts.at("2")->get().height==20&&
            take(list->row("0")).snapshot()->bounds.height==60&&take(list->row("1")).snapshot()->bounds.y==60&&
            take(list->root().snapshot()).content_height==455,
            "Natural custom row heights did not update the viewport");
    auto one=contexts.at("1");auto identity=take(roots.at("1").placement()).scope.lock();
    good(list->root().set_scroll_offset(75));(void)take(list->root().snapshot());
    require(take(list->root().snapshot()).scroll_offset==75,"The variable-height anchor was not established");
    good(roots.at("0").set_extent({200,80}));(void)take(tree->drawing_snapshot({200,120}));
    require(take(list->root().snapshot()).scroll_offset==95&&contexts.at("1")==one&&one->get().height==35&&
            take(roots.at("1").placement()).scope.lock()==identity&&take(source->snapshot())->revision==0,
            "Changing an overscanned row height moved the visible anchor or replaced its view");
    good(roots.at("0").set_extent({200,10}));(void)take(list->root().snapshot());
    require(take(list->root().snapshot()).scroll_offset==35&&contexts.at("0")->get().height==20&&contexts.at("1")==one,
            "Shrinking a delegate below the minimum lost the anchor or the minimum row height");
    input.row_height=30;good(list->set_options(input));(void)take(list->root().snapshot());
    require(take(list->root().snapshot()).scroll_offset==45&&contexts.at("0")->get().height==30&&one->get().height==35,
            "Changing the base row height lost the measured extent or anchor");
    input.width=180;good(list->set_options(input));(void)take(list->root().snapshot());
    require(contexts.at("1")==one&&one->get().height==35&&take(source->snapshot())->revision==0,
            "Resizing the list replaced a retained row or changed the model");
    good(host.clear_content());tree->close();
}
void pointer_and_retirement(){
    using namespace ii::gui;using Row=lvui::native::ListRowContext;
    auto source=take(lvui::ListMemory::create({{.key="one",.label="One"},{.key="two",.label="Two"}}));
    auto controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    lvui::native::ListViewInput input;input.controller=controller;input.width=220;input.height=44;input.row_height=22;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"pointer-host",{240,80}));std::optional<ListViewTestView> list;
    std::shared_ptr<const Row> one;Widget button;int list_clicks=0,button_clicks=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto view=ListViewTestView::create(builder,{},"list",input,{.activated=[&](const auto&){++list_clicks;}},
            [&](WidgetBuilder& child,std::shared_ptr<const Row> row)->ii::core::Result<void>{
                const auto key=row->get().entry.key;if(key=="one")one=row;
                auto root=child.absolute_stack({},"custom",{220,22});if(!root)return std::unexpected(root.error());
                auto label=child.label(*root,"text",{.text=key});if(!label)return std::unexpected(label.error());
                if(auto placed=label->set_frame({0,0,100,22});!placed)return placed;
                auto action=child.button(*root,"action",{.text="Act",.height=22,.width=70},[&]{++button_clicks;});
                if(!action)return std::unexpected(action.error());if(auto placed=action->set_frame({120,0,70,22});!placed)return placed;
                if(key=="one")button=*action;return {};
            });
        if(!view)return std::unexpected(view.error());list=*view;return {};
    }));
    (void)take(tree->drawing_snapshot({240,80}));auto row=take(list->row("one"));auto bounds=take(row.snapshot()).bounds;
    const auto click=[&](double x,double y){good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y,.buttons=1}));
        good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));};
    click(bounds.x+20,bounds.y+10);require(list_clicks==1&&button_clicks==0&&one->get().selected,
        "A passive custom row did not pass clicks to the selection surface");
    const auto action=take(button.snapshot()).bounds;click(action.x+10,action.y+10);
    require(list_clicks==1&&button_clicks==1&&take(controller->selection())=="one",
        "An embedded button did not keep its own pointer action");
    good(row.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));
    require(take(controller->selection())=="two"&&!one->get().selected&&list_clicks==2,
        "Keyboard navigation did not update a custom row context");
    auto item=take(source->snapshot())->entries[0];item.enabled=false;
    input.source_revision=std::int64_t(take(source->apply({.revision=0,.kind=lvui::ListEditKind::replace,.key="one",.item=item})));
    good(list->set_options(input));require(!one->get().enabled&&!take(button.interaction()).enabled,
        "Disabling a row did not disable its delegate controls");
    bool retired=false;auto subscription=one->observe([&](const auto& value,std::uint64_t){
        if(value.entry.label=="Retire"){retired=true;good(host.clear_content());}
    });
    item.label="Retire";input.source_revision=std::int64_t(take(source->apply({.revision=1,.kind=lvui::ListEditKind::replace,.key="one",.item=item})));
    (void)list->set_options(input);require(retired&&!row.snapshot(),"A row observer could not safely retire its list");
    bool closed=false;try{(void)one->get();}catch(const std::logic_error&){closed=true;}
    require(closed,"A row context survived retirement inside its observer");tree->close();
}
void reentrant_factory(){
    using namespace ii::gui;using Row=lvui::native::ListRowContext;
    auto source=take(lvui::ListMemory::create({{.key="one",.label="One"},{.key="two",.label="Two"}}));
    auto controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    lvui::native::ListViewInput input;input.controller=controller;input.width=220;input.height=40;input.overscan=1;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"reentrant-host",{240,80}));std::optional<ListViewTestView> list;
    std::map<std::string,std::shared_ptr<const Row>> contexts;bool reenter=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto view=ListViewTestView::create(builder,{},"list",input,{},
            [&](WidgetBuilder& child,std::shared_ptr<const Row> row)->ii::core::Result<void>{
                const auto key=row->get().entry.key;contexts[key]=row;
                if(key=="new"&&std::exchange(reenter,false)){
                    auto next=take(source->snapshot())->entries[1];next.label="Newer";
                    input.source_revision=std::int64_t(take(source->apply({.revision=1,.kind=lvui::ListEditKind::replace,.key="one",.item=next})));
                    if(auto refreshed=list->set_options(input);!refreshed)return refreshed;
                }
                auto label=child.label({},"label",{.text=key});if(!label)return std::unexpected(label.error());return {};
            });
        if(!view)return std::unexpected(view.error());list=*view;return {};
    }));
    (void)take(tree->drawing_snapshot({240,80}));(void)take(list->root().snapshot());
    reenter=true;input.source_revision=std::int64_t(take(source->apply({.revision=0,.kind=lvui::ListEditKind::insert,.before="one",.item={.key="new",.label="New"}})));
    good(list->set_options(input));
    require(!reenter&&contexts.contains("new")&&contexts.at("one")->get().entry.label=="Newer"&&list->row("new"),
            "A row factory's reentrant update lost the published row or the newer model");
    good(host.clear_content());tree->close();
}
void reentrant_external_refresh(){
    using namespace ii::gui;using Row=lvui::native::ListRowContext;
    auto source=take(lvui::ListMemory::create({{.key="one",.label="One"},{.key="two",.label="Two"}}));
    auto controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    lvui::native::ListViewInput input;input.controller=controller;input.width=220;input.height=40;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"external-host",{240,80}));std::optional<ListViewTestView> list;
    std::map<std::string,std::shared_ptr<const Row>> contexts;int activations=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto view=ListViewTestView::create(builder,{},"list",input,{.activated=[&](const auto&){++activations;}},
            [&](WidgetBuilder& child,std::shared_ptr<const Row> row)->ii::core::Result<void>{
                const auto key=row->get().entry.key;contexts[key]=std::move(row);
                auto label=child.label({},"label",{.text=key});if(!label)return std::unexpected(label.error());return {};
            });
        if(!view)return std::unexpected(view.error());list=*view;return {};
    }));
    (void)take(tree->drawing_snapshot({240,80}));(void)take(list->root().snapshot());
    auto first=contexts.at("one"),second=contexts.at("two");bool reentered=false;std::vector<std::string> observed;
    auto second_watch=second->observe([&](const auto& value,std::uint64_t){observed.push_back(value.entry.label);});
    auto first_watch=first->observe([&](const auto& value,std::uint64_t){
        if(value.entry.label=="Older"&&!reentered){reentered=true;auto item=take(source->snapshot())->entries[1];item.label="Newer";
            (void)take(source->apply({.revision=2,.kind=lvui::ListEditKind::replace,.key="two",.item=item}));
            good(take(list->row("two")).activate());}
    });
    auto one=take(source->snapshot())->entries[0];one.label="Older";
    auto revision=take(source->apply({.revision=0,.kind=lvui::ListEditKind::replace,.key="one",.item=one}));
    auto two=take(source->snapshot())->entries[1];two.label="Stale";
    revision=take(source->apply({.revision=revision,.kind=lvui::ListEditKind::replace,.key="two",.item=two}));input.source_revision=std::int64_t(revision);
    good(list->set_options(input));
    require(reentered&&second->get().entry.label=="Newer"&&observed==std::vector<std::string>{"Newer"}&&activations==0,
            "A stale external refresh published older row data or delivered a stale activation");
    good(host.clear_content());tree->close();
}
void selection_observer_failure(){
    using namespace ii::gui;using Row=lvui::native::ListRowContext;
    auto source=take(lvui::ListMemory::create({{.key="one",.label="One"},{.key="two",.label="Two"}}));
    auto controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    lvui::native::ListViewInput input;input.controller=controller;input.width=220;input.height=40;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"failure-host",{240,80}));std::optional<ListViewTestView> list;
    std::map<std::string,std::shared_ptr<const Row>> contexts;int activations=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto view=ListViewTestView::create(builder,{},"list",input,{.activated=[&](const auto&){++activations;}},
            [&](WidgetBuilder& child,std::shared_ptr<const Row> row)->ii::core::Result<void>{
                const auto key=row->get().entry.key;contexts[key]=std::move(row);
                auto label=child.label({},"label",{.text=key});if(!label)return std::unexpected(label.error());return {};
            });
        if(!view)return std::unexpected(view.error());list=*view;return {};
    }));
    (void)take(tree->drawing_snapshot({240,80}));(void)take(list->root().snapshot());
    auto one=contexts.at("one"),two=contexts.at("two");good(take(list->row("one")).activate());
    auto failing=one->observe([](const auto& value,std::uint64_t){if(!value.selected)throw std::runtime_error("Selection observer failed");});
    good(take(list->row("two")).activate());
    require(take(controller->selection())=="two"&&!one->get().selected&&two->get().selected&&
            take(list->failure()).has_value()&&activations==1&&take(source->snapshot())->revision==0,
            "A failing selection observer left row selection states inconsistent");
    good(host.clear_content());tree->close();
}
}
int main(){
    using namespace ii::gui;using Row=lvui::native::ListRowContext;
    std::vector<lvui::ListEntry> entries{{.key="one",.label="One"},{.key="two",.label="Two"}};
    for(int i=0;i<30;++i)entries.push_back({.key="row"+std::to_string(i),.label="Row "+std::to_string(i)});
    auto source=take(lvui::ListMemory::create(entries));
    auto controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    lvui::native::ListViewInput input;input.controller=controller;input.width=220;input.height=40;input.overscan=0;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{260,80}));std::optional<ListViewTestView> list;
    std::map<std::string,std::shared_ptr<const Row>> contexts;std::map<std::string,Widget> labels;
    std::shared_ptr<const Row> rejected;bool reject_bad=false;int activations=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto view=ListViewTestView::create(builder,{},"list",input,{.activated=[&](const auto&){++activations;}},
            [&](WidgetBuilder& child,std::shared_ptr<const Row> row)->ii::core::Result<void>{
                const auto initial=row->get();const auto key=initial.entry.key;
                if(reject_bad&&key=="bad"){rejected=row;return ii::core::fail(ii::core::ErrorCode::invalid_argument,"Rejected row View");}
                auto label=child.label({},"custom",{.text="Custom "+initial.entry.label});if(!label)return std::unexpected(label.error());
                auto subscription=std::make_shared<ii::core::ObservableSubscription>(row->observe(
                    [label=*label](const auto& next,std::uint64_t){good(label.set_text("Custom "+next.entry.label));}));
                if(auto retained=child.retain(subscription);!retained)return retained;
                contexts[key]=std::move(row);labels[key]=*label;return {};
            });
        if(!view)return std::unexpected(view.error());list=*view;return {};
    }));
    (void)take(tree->drawing_snapshot({260,80}));(void)take(list->root().snapshot());
    auto drawing=take(tree->drawing_snapshot({260,80}));
    require(contexts.contains("one")&&contexts.contains("two")&&text_present(drawing,"Custom One")&&!text_present(drawing,"One"),
            "Custom rows were not mounted or retained the default text");
    auto one=contexts.at("one"),two=contexts.at("two");auto label=labels.at("one");
    const auto identity=take(label.placement()).scope.lock();
    require(one->get().source_index==0&&!one->get().selected&&one->get().enabled&&one->get().height==20,
            "The initial flat-list row context is incorrect");
    lvui::native::Bindings bindings;std::string observed;
    require(bool(bindings.bind([one](lvui::native::ReadContext& ctx){return ctx.read(*one).entry.label;},
        [&](std::string value)->ii::core::Result<void>{observed=std::move(value);return {};}))&&observed=="One",
        "The flat-list row context did not expose its reactive dependency");
    input.row_height=30;good(list->set_options(input));
    require(one->get().height==30&&take(take(list->row("one")).snapshot()).bounds.height==30&&
            take(label.placement()).scope.lock()==identity&&take(source->snapshot())->revision==0,
            "A row-height change recreated its delegate or mutated the source");
    input.row_height=20;good(list->set_options(input));
    require(one->get().height==20&&take(label.placement()).scope.lock()==identity,"Resetting row height lost the retained delegate");
    std::uint64_t revision=0;
    auto replacement=entries[0];replacement.label="Updated";replacement.icon_name="unused-missing-icon";
    revision=take(source->apply({.revision=revision,.kind=lvui::ListEditKind::replace,.key="one",.item=replacement}));input.source_revision=std::int64_t(revision);
    good(list->set_options(input));
    require(contexts.at("one")==one&&one->get().entry.label=="Updated"&&observed=="Updated"&&
            take(label.snapshot()).text=="Custom Updated"&&take(label.placement()).scope.lock()==identity,
            "Updating a visible key replaced its delegate or resolved an unused default icon");
    auto invalid=input;invalid.view_revision=-1;
    require(!list->set_options(invalid)&&one->get().entry.label=="Updated"&&take(label.snapshot()).text=="Custom Updated",
            "An invalid view revision replaced the current row");
    good(take(list->row("one")).activate());
    require(one->get().selected&&!two->get().selected&&activations==1&&take(source->snapshot())->revision==revision,
            "Selecting a row did not update its context without editing the source");
    input.overscan=1;
    revision=take(source->apply({.revision=revision,.kind=lvui::ListEditKind::move,.key="two",.before="one"}));input.source_revision=std::int64_t(revision);
    good(list->set_options(input));good(list->root().set_scroll_offset(0));
    require(one->get().source_index==1&&two->get().source_index==0&&take(label.placement()).scope.lock()==identity,
            "Reordering did not retain the row context and update its index");
    bool reentered=false;auto reentrant=one->observe([&](const auto& value,std::uint64_t){
        if(value.entry.label=="Older"&&!reentered){reentered=true;replacement.label="Newer";
            revision=take(source->apply({.revision=revision,.kind=lvui::ListEditKind::replace,.key="one",.item=replacement}));
            input.source_revision=std::int64_t(revision);good(list->set_options(input));}
    });
    replacement.label="Older";revision=take(source->apply({.revision=revision,.kind=lvui::ListEditKind::replace,.key="one",.item=replacement}));
    input.source_revision=std::int64_t(revision);good(list->set_options(input));
    require(reentered&&one->get().entry.label=="Newer"&&observed=="Newer"&&take(label.snapshot()).text=="Custom Newer",
            "Reentrant row data was overwritten by an older update");
    {
        auto failing=one->observe([](const auto& value,std::uint64_t){if(value.entry.label=="Reject")throw std::runtime_error("Observer rejected update");});
        replacement.label="Reject";revision=take(source->apply({.revision=revision,.kind=lvui::ListEditKind::replace,.key="one",.item=replacement}));
        input.source_revision=std::int64_t(revision);
        require(!list->set_options(input)&&one->get().entry.label=="Newer"&&observed=="Newer"&&take(label.snapshot()).text=="Custom Newer",
                "A failed row observer left a partially published view");
    }
    replacement.label="Newer";revision=take(source->apply({.revision=revision,.kind=lvui::ListEditKind::replace,.key="one",.item=replacement}));
    input.source_revision=std::int64_t(revision);good(list->set_options(input));
    bool wrong=false;std::thread worker([&]{try{(void)one->get();}catch(const std::logic_error&){wrong=true;}});worker.join();
    require(wrong,"The row context accepted a foreign-thread read");
    reject_bad=true;revision=take(source->apply({.revision=revision,.kind=lvui::ListEditKind::insert,.before="two",.item={.key="bad",.label="Bad"}}));
    input.source_revision=std::int64_t(revision);
    require(!list->set_options(input)&&one->get().entry.label=="Newer"&&take(label.snapshot()).text=="Custom Newer",
            "A rejected row delegate replaced a previously valid view");
    require(bool(rejected),"The rejecting row factory was not invoked");bool rejected_closed=false;
    try{(void)rejected->get();}catch(const std::logic_error&){rejected_closed=true;}require(rejected_closed,"Rejected row context remained live");
    reject_bad=false;revision=take(source->apply({.revision=revision,.kind=lvui::ListEditKind::erase,.key="bad"}));
    input.source_revision=std::int64_t(revision);good(list->set_options(input));
    good(list->root().set_scroll_offset(400));bool closed=false;try{(void)one->get();}catch(const std::logic_error&){closed=true;}
    require(closed&&!label.snapshot(),"A virtualized-offscreen row retained its view or context");
    good(bindings.close());good(host.clear_content());tree->close();row_trigger();row_trigger_retires();variable_row_height();pointer_and_retirement();reentrant_factory();reentrant_external_refresh();selection_observer_failure();
}
