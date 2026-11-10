#include "list_view_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
template<class T>T take(lvui::ListResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value)throw std::runtime_error(value.error().message);}
class RejectEditor final:public lvui::ListEditor {
public:lvui::ListResult<std::uint64_t> apply(const lvui::ListEdit&)override{
    return std::unexpected(lvui::ListError{lvui::ListErrorCode::invalid,"Move rejected"});}
};
class RetiringEditor final:public lvui::ListEditor {
public:std::shared_ptr<lvui::ListMemory> source;ii::gui::Widget host;
    lvui::ListResult<std::uint64_t> apply(const lvui::ListEdit& edit)override{
        auto result=source->apply(edit);if(result)good(host.clear_content());return result;}
};
std::vector<std::string> keys(const lvui::ListSnapshot& snapshot){std::vector<std::string> out;for(const auto& e:snapshot.entries)out.push_back(e.key);return out;}
bool indicator(const ii::gui::CanvasFrame& frame){
    const auto has=[](const auto& panels){return std::ranges::any_of(panels,[](const auto& panel){return panel.bounds.width>=100&&panel.bounds.height==2;});};
    if(has(frame.panels))return true;for(const auto& layer:frame.overlays)if(has(layer.panels))return true;return false;
}
void custom_and_scrolling(){
    using namespace ii::gui;using Row=lvui::native::ListRowContext;
    std::vector<lvui::ListEntry> entries;for(int i=0;i<20;++i)entries.push_back({.key=std::to_string(i),.label="Item"});
    auto source=take(lvui::ListMemory::create(entries));
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"custom-host",{200,100}));std::optional<ListViewTestView> list;
    lvui::native::ListViewInput input;input.controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    input.editor=source;input.reorderable=true;input.width=200;input.height=100;input.overscan=0;
    int buttons=0;std::vector<lvui::ListDragProposal> moves;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=ListViewTestView::create(builder,{},"list",input,{.moved=[&](const auto& event){moves.push_back(event);}},
            [&](WidgetBuilder& child,std::shared_ptr<const Row> row)->ii::core::Result<void>{
                const auto height=row->get().entry.key=="0"?60:row->get().entry.key=="1"?35:20;
                auto content=child.absolute_stack({},"content",{200,double(height)});if(!content)return std::unexpected(content.error());
                auto action=child.button(*content,"button",{.text="Act",.height=20,.width=60},[&]{++buttons;});
                if(!action)return std::unexpected(action.error());return action->set_frame({120,0,60,20});
            });
        if(!value)return std::unexpected(value.error());list=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({200,100}));(void)take(list->root().snapshot());
    const auto send=[&](WidgetInputKind kind,double x,double y){good(tree->input({.kind=kind,.x=x,.y=y,.buttons=1}));};
    send(WidgetInputKind::pointer_down,130,10);send(WidgetInputKind::pointer_up,130,10);
    require(buttons==1&&take(source->snapshot())->revision==0,"List dragging intercepted a nested button");
    send(WidgetInputKind::pointer_down,10,10);send(WidgetInputKind::pointer_move,10,79);
    require(indicator(take(tree->drawing_snapshot({200,100})))&&take(source->snapshot())->revision==0,
        "Variable-height drag did not show a preview without editing");
    send(WidgetInputKind::pointer_up,10,79);
    require(take(source->snapshot())->revision==1&&take(source->snapshot())->entries[1].key=="0"&&
        moves.size()==1&&moves[0].to==1&&!indicator(take(tree->drawing_snapshot({200,100}))),
        "Variable-height drag chose the wrong drop or kept its preview");
    good(host.clear_content());
    source=take(lvui::ListMemory::create(entries));input.controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    input.editor=source;moves.clear();
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=ListViewTestView::create(builder,{},"long-list",input,{.moved=[&](const auto& event){moves.push_back(event);}});
        if(!value)return std::unexpected(value.error());list=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({200,100}));(void)take(list->root().snapshot());
    input.scrollable=false;good(list->set_options(input));
    send(WidgetInputKind::pointer_down,10,10);send(WidgetInputKind::pointer_move,10,95);
    require(take(list->root().snapshot()).scroll_offset==0&&take(source->snapshot())->revision==0,
        "A non-scrollable list moved during edge dragging");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    send(WidgetInputKind::pointer_up,10,95);input.scrollable=true;good(list->set_options(input));
    send(WidgetInputKind::pointer_down,10,10);send(WidgetInputKind::pointer_move,10,95);
    const auto scroll=take(list->root().snapshot()).scroll_offset;const auto revision=take(source->snapshot())->revision;
    if(scroll!=20||revision!=0)throw std::runtime_error("Dragging at the edge: offset="+std::to_string(scroll)+" revision="+std::to_string(revision)+
        " selection="+take(input.controller->selection())+" content="+std::to_string(take(list->root().snapshot()).content_height)+
        " indicator="+std::to_string(indicator(take(tree->drawing_snapshot({200,100})))));
    send(WidgetInputKind::pointer_up,10,95);
    require(take(source->snapshot())->revision==1&&take(source->snapshot())->entries[5].key=="0"&&moves.size()==1&&moves[0].to==5,
        "Dropping after edge scrolling used a stale viewport target");
    good(host.clear_content());tree->close();
}
void selection_reconfigures_drag(){
    using namespace ii::gui;using Row=lvui::native::ListRowContext;
    auto source=take(lvui::ListMemory::create({{.key="a",.label="A"},{.key="b",.label="B"}}));
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"reentrant-host",{200,60}));std::optional<ListViewTestView> list;
    lvui::native::ListViewInput input;input.controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    input.editor=source;input.reorderable=true;input.width=200;input.height=60;
    std::shared_ptr<const Row> first;bool changed=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=ListViewTestView::create(builder,{},"list",input,{},
            [&](WidgetBuilder& child,std::shared_ptr<const Row> row)->ii::core::Result<void>{
                if(row->get().entry.key=="a")first=row;auto content=child.absolute_stack({},"content",{200,20});
                return content?ii::core::Result<void>{}:std::unexpected(content.error());
            });
        if(!value)return std::unexpected(value.error());list=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({200,60}));(void)take(list->root().snapshot());
    auto observer=first->observe([&](const auto& value,std::uint64_t){if(value.selected&&!changed){changed=true;input.reorderable=false;good(list->set_options(input));}});
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=10,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_move,.x=10,.y=38,.buttons=1}));
    require(changed&&!indicator(take(tree->drawing_snapshot({200,60})))&&take(source->snapshot())->revision==0,
        "A selection observer disabled reordering but the old drag still started");
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=10,.y=38,.buttons=1}));
    require(take(source->snapshot())->revision==0,"A retired drag still committed after reconfiguration");
    good(host.clear_content());tree->close();
}
void shared_controller_before_press(){
    using namespace ii::gui;
    auto source=take(lvui::ListMemory::create({{.key="a",.label="A"},{.key="b",.label="B"}}));
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"shared-host",{200,60}));std::optional<ListViewTestView> list;
    lvui::native::ListViewInput input;input.controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    input.editor=source;input.reorderable=true;input.width=200;input.height=60;int moves=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=ListViewTestView::create(builder,{},"list",input,{.moved=[&](const auto&){++moves;}});
        if(!value)return std::unexpected(value.error());list=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({200,60}));(void)take(list->root().snapshot());
    auto item=take(source->snapshot())->entries[0];item.label="Updated";
    (void)take(source->apply({.revision=0,.kind=lvui::ListEditKind::replace,.key="a",.item=item}));
    require(take(input.controller->refresh()),"The shared controller did not refresh before the press");
    const auto send=[&](WidgetInputKind kind,double y){good(tree->input({.kind=kind,.x=10,.y=y,.buttons=1}));};
    send(WidgetInputKind::pointer_down,10);send(WidgetInputKind::pointer_move,38);send(WidgetInputKind::pointer_up,38);
    require(take(source->snapshot())->revision==1&&moves==0&&take(take(list->row("a")).action_options()).accessibility_name=="Updated",
        "A stale press moved data or failed to show the shared controller's newer snapshot");
    send(WidgetInputKind::pointer_down,10);send(WidgetInputKind::pointer_move,38);send(WidgetInputKind::pointer_up,38);
    require(take(source->snapshot())->revision==2&&take(source->snapshot())->entries[1].key=="a"&&moves==1,
        "The refreshed list did not accept a later drag");
    good(host.clear_content());tree->close();
}
}
int main(){
    using namespace ii::gui;
    auto source=take(lvui::ListMemory::create({{.key="a",.label="A"},{.key="b",.label="B"},
        {.key="c",.label="C"},{.key="fixed",.label="Fixed",.draggable=false},
        {.key="disabled",.label="Disabled",.enabled=false}}));
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));
    lvui::native::ListViewInput input;input.controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    input.editor=source;input.editable=true;input.reorderable=true;input.width=200;input.height=100;input.row_height=20;input.overscan=0;
    std::optional<ListViewTestView> list;std::vector<std::string> activated;std::vector<lvui::ListDragProposal> moved;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=ListViewTestView::create(builder,{},"list",input,
            {.activated=[&](const auto& event){activated.push_back(event.entry.key);},.moved=[&](const auto& event){moved.push_back(event);}});
        if(!value)return std::unexpected(value.error());list=*value;return {};
    }));
    (void)take(tree->drawing_snapshot({200,100}));(void)take(list->root().snapshot());
    const auto send=[&](WidgetInputKind kind,double x,double y,unsigned clicks=0){return tree->input({.kind=kind,.x=x,.y=y,.buttons=1,.native_click_count=clicks});};
    good(send(WidgetInputKind::pointer_down,10,10));good(send(WidgetInputKind::pointer_up,10,10));
    require(activated==std::vector<std::string>{"a"}&&take(source->snapshot())->revision==0,
        "A click in a reorderable list did not activate once without editing");
#if defined(IIGUI_POINTER_CANCEL_API)
    good(send(WidgetInputKind::pointer_down,10,10));good(send(WidgetInputKind::pointer_move,10,58));
    good(send(WidgetInputKind::pointer_cancel,10,58));good(send(WidgetInputKind::pointer_up,10,58));
    require(take(source->snapshot())->revision==0&&moved.empty()&&!indicator(take(tree->drawing_snapshot({200,100}))),
        "Pointer cancellation committed a list reorder or left its preview");
#endif
    good(send(WidgetInputKind::pointer_down,10,10));good(send(WidgetInputKind::pointer_move,10,58));
    require(take(source->snapshot())->revision==0&&indicator(take(tree->drawing_snapshot({200,100}))),"A drag preview changed the source or omitted its indicator");
    require(!list->begin_edit()&&!list->editor(),"A rename started while a drag was active");
    good(send(WidgetInputKind::pointer_up,10,58));auto snapshot=take(source->snapshot());
    require(snapshot->revision==1&&keys(*snapshot)==std::vector<std::string>({"b","c","a","fixed","disabled"})&&
        moved.size()==1&&moved[0].request.key=="a"&&moved[0].from==0&&moved[0].to==2&&
        take(input.controller->selection())=="a"&&activated==std::vector<std::string>{"a"},
        "A drop failed to commit one move, retain selection or avoid activation");
    good(send(WidgetInputKind::pointer_down,10,10,2));good(send(WidgetInputKind::pointer_up,10,10,2));
    require(bool(list->editor()),"Double-click rename stopped working when reordering was enabled");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    require(!list->editor()&&take(source->snapshot())->revision==1,"Canceling a rename changed the order");
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=10,.timestamp=1.0,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=10,.y=10,.timestamp=1.01}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=10,.timestamp=1.2,.buttons=1}));
    const bool after_second_down=bool(list->editor());
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=10,.y=10,.timestamp=1.21}));
    if(!list->editor())throw std::runtime_error("Timed double-click rename stopped working; after down="+std::to_string(after_second_down)+
        " selection="+take(input.controller->selection())+" revision="+std::to_string(take(source->snapshot())->revision));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=30,.timestamp=2.0,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=10,.y=30,.timestamp=2.01}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=30,.timestamp=2.2,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=10,.y=30,.timestamp=2.21}));
    require(bool(list->editor())&&take(input.controller->selection())=="c",
        "Timed double-click did not edit a row below the viewport origin");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));
    good(send(WidgetInputKind::pointer_down,10,10));good(send(WidgetInputKind::pointer_move,10,58));
    auto changed=take(source->snapshot())->entries[1];changed.label="Fresh";
    (void)take(source->apply({.revision=1,.kind=lvui::ListEditKind::replace,.key=changed.key,.item=changed}));
    require(take(input.controller->refresh()),"Shared controller did not observe the external update");
    good(send(WidgetInputKind::pointer_up,10,58));
    require(take(source->snapshot())->revision==2&&moved.size()==1&&take(take(list->row("c")).action_options()).accessibility_name=="Fresh",
        "A stale drag moved newer data or did not refresh it");
    input.source_revision=2;good(list->set_options(input));
    auto missing=input;missing.editor.reset();missing.editable=false;
    require(!list->set_options(missing),"A reorderable list accepted a missing editor");
    good(send(WidgetInputKind::pointer_down,10,70));good(send(WidgetInputKind::pointer_move,10,38));good(send(WidgetInputKind::pointer_up,10,38));
    require(take(source->snapshot())->revision==2&&moved.size()==1,"A fixed row was moved");
    good(send(WidgetInputKind::pointer_down,10,10));good(send(WidgetInputKind::pointer_move,10,38));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::escape}));good(send(WidgetInputKind::pointer_up,10,38));
    require(take(source->snapshot())->revision==2&&moved.size()==1,"Escape failed to cancel a drag");
    good(send(WidgetInputKind::pointer_down,10,10));good(send(WidgetInputKind::pointer_move,10,38));
    good(tree->input({.kind=WidgetInputKind::focus_lost}));good(send(WidgetInputKind::pointer_up,10,38));
    require(take(source->snapshot())->revision==2&&moved.size()==1,"Focus loss failed to cancel a drag");
    good(send(WidgetInputKind::pointer_down,10,10));good(send(WidgetInputKind::pointer_move,10,38));
    input.reorderable=false;good(list->set_options(input));good(send(WidgetInputKind::pointer_up,10,38));
    require(take(source->snapshot())->revision==2&&moved.size()==1,"Disabling reordering failed to cancel a drag");
    input.reorderable=true;input.editor=std::make_shared<RejectEditor>();good(list->set_options(input));
    good(send(WidgetInputKind::pointer_down,10,10));good(send(WidgetInputKind::pointer_move,10,38));
    require(!send(WidgetInputKind::pointer_up,10,38)&&take(source->snapshot())->revision==2&&moved.size()==1,
        "A rejected move changed the source or emitted a move");
    auto retiring=std::make_shared<RetiringEditor>();retiring->source=source;retiring->host=host;input.editor=retiring;good(list->set_options(input));
    good(send(WidgetInputKind::pointer_down,10,10));good(send(WidgetInputKind::pointer_move,10,38));good(send(WidgetInputKind::pointer_up,10,38));
    require(take(source->snapshot())->revision==3&&moved.size()==1&&!list->set_options(input),
        "A retiring list editor emitted a stale callback or retained the view");
    tree->close();custom_and_scrolling();selection_reconfigures_drag();shared_controller_before_press();
}
