#include "list_view_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
template<class T>T take(lvui::ListResult<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
class ValidatingEditor final:public lvui::ListEditor {
public:
    std::shared_ptr<lvui::ListMemory> source;
    lvui::ListResult<std::uint64_t> apply(const lvui::ListEdit& edit)override{
        if(edit.item.label=="forbidden")return std::unexpected(lvui::ListError{lvui::ListErrorCode::invalid,"Name rejected"});
        return source->apply(edit);
    }
};
class RetiringEditor final:public lvui::ListEditor {
public:
    std::shared_ptr<lvui::ListMemory> source;ii::gui::Widget host;
    lvui::ListResult<std::uint64_t> apply(const lvui::ListEdit& edit)override{
        auto result=source->apply(edit);if(result)good(host.clear_content());return result;
    }
};
void embedded_controls(){
    using namespace ii::gui;
    auto source=take(lvui::ListMemory::create({{.key="a",.label="Alpha"}}));
    lvui::native::ListViewInput input;input.controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    input.editor=source;input.editable=true;input.width=200;input.height=40;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"nested-host",{200,60}));std::optional<ListViewTestView> list;
    Widget button;int actions=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto view=ListViewTestView::create(builder,{},"list",input,{},
            [&](WidgetBuilder& child,std::shared_ptr<const lvui::native::ListRowContext>)->ii::core::Result<void>{
                auto root=child.absolute_stack({},"content",{200,20});if(!root)return std::unexpected(root.error());
                auto action=child.button(*root,"button",{.text="Action",.height=20,.width=80},[&]{++actions;});
                if(!action)return std::unexpected(action.error());if(auto placed=action->set_frame({0,0,80,20});!placed)return placed;
                button=*action;return {};
            });
        if(!view)return std::unexpected(view.error());list=*view;return {};
    }));
    (void)take(tree->drawing_snapshot({200,60}));(void)take(list->root().snapshot());
    good(take(list->row("a")).activate());good(button.focus());
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(tree->input({.kind=WidgetInputKind::key_up,.key=WidgetKey::enter}));
    if(actions!=1||list->editor())throw std::runtime_error("Enter on a nested button: actions="+std::to_string(actions)+
        " editing="+std::to_string(bool(list->editor()))+" focused="+std::to_string(take(button.interaction()).focused));
    const auto bounds=take(button.snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=bounds.x+10,.y=bounds.y+10,.buttons=1,.native_click_count=2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=bounds.x+10,.y=bounds.y+10,.native_click_count=2}));
    require(actions==2&&!list->editor(),"Double-click on a nested button opened a list editor");
    good(host.clear_content());tree->close();
}
void shared_controller_editing(){
    using namespace ii::gui;
    auto source=take(lvui::ListMemory::create({{.key="a",.label="Old"},{.key="b",.label="Old B"}}));
    auto controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    lvui::native::ListViewInput input;input.controller=controller;input.editor=source;input.editable=true;input.width=200;input.height=40;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"shared-host",{200,40}));std::optional<ListViewTestView> list;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto view=ListViewTestView::create(builder,{},"list",input);
        if(!view)return std::unexpected(view.error());list=*view;return {};
    }));
    (void)take(tree->drawing_snapshot({200,40}));(void)take(list->root().snapshot());good(take(list->row("a")).activate());
    auto item=take(source->snapshot())->entries[0];item.label="Newer";
    (void)take(source->apply({.revision=0,.kind=lvui::ListEditKind::replace,.key="a",.item=item}));
    require(take(controller->refresh()),"The shared editing controller did not refresh");
    auto stale=list->begin_edit();
    require(!stale&&stale.error().code==ii::core::ErrorCode::busy&&!list->editor()&&
        take(take(list->row("a")).action_options()).accessibility_name=="Newer",
        "A shared controller allowed editing an older visible snapshot");
    good(list->begin_edit());require(take(take(list->editor()).snapshot()).text=="Newer","A fresh edit did not use the updated label");
    good(list->cancel_edit());item.label="Newest";
    (void)take(source->apply({.revision=1,.kind=lvui::ListEditKind::replace,.key="a",.item=item}));
    require(take(controller->refresh()),"The shared keyboard editing controller did not refresh");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(!list->editor()&&take(take(list->row("a")).action_options()).accessibility_name=="Newest",
        "A stale rename shortcut did not refresh and discard its old event");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(take(take(list->editor()).snapshot()).text=="Newest","A fresh rename shortcut did not open the updated item");
    good(list->cancel_edit());auto other=take(source->snapshot())->entries[1];other.label="New B";
    (void)take(source->apply({.revision=2,.kind=lvui::ListEditKind::replace,.key="b",.item=other}));
    require(take(controller->refresh()),"The shared pointer editing controller did not refresh");
    const auto double_click=[&]{good(tree->input({.kind=WidgetInputKind::pointer_down,.x=10,.y=30,.buttons=1,.native_click_count=2}));
        good(tree->input({.kind=WidgetInputKind::pointer_up,.x=10,.y=30,.native_click_count=2}));};
    double_click();require(!list->editor()&&take(controller->selection())=="a"&&
        take(take(list->row("b")).action_options()).accessibility_name=="New B",
        "A stale double-click changed selection instead of refreshing");
    double_click();require(take(take(list->editor()).snapshot()).text=="New B"&&take(controller->selection())=="b",
        "A fresh double-click did not edit the updated row");
    good(list->cancel_edit());good(host.clear_content());tree->close();
}
}
int main(){
    using namespace ii::gui;
    std::vector<lvui::ListEntry> entries{{.key="a",.label="Alpha"},{.key="locked",.label="Locked",.editable=false}};
    for(int i=0;i<30;++i)entries.push_back({.key="r"+std::to_string(i),.label="Row"});
    auto source=take(lvui::ListMemory::create(std::move(entries)));auto editor=std::make_shared<ValidatingEditor>();editor->source=source;
    lvui::native::ListViewInput input;input.controller=std::make_shared<lvui::ListController>(take(lvui::ListController::create(source)));
    input.editor=editor;input.editable=true;input.width=200;input.height=80;input.row_height=20;input.overscan=0;
    auto tree=std::make_shared<detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{200,100}));std::optional<ListViewTestView> list;
    std::vector<std::string> drafts,submitted,rejected;std::optional<lvui::native::ListEditEvent> latest;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto view=ListViewTestView::create(builder,{},"list",input,
            {.edited=[&](const auto& event){drafts.push_back(event.text);latest=event;},
             .submitted=[&](const auto& event){submitted.push_back(event.text);},
             .rejected=[&](const auto& event,const auto&){rejected.push_back(event.text);}});
        if(!view)return std::unexpected(view.error());list=*view;return {};
    }));
    (void)take(tree->drawing_snapshot({200,100}));(void)take(list->root().snapshot());
    const auto click=[&](double x,double y){good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y,.buttons=1}));
        good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));};
    const auto key=[&](WidgetKey value){good(tree->input({.kind=WidgetInputKind::key_down,.key=value}));};
    const auto replace=[&](std::string text){auto field=take(list->editor());auto current=take(field.snapshot());
        good(field.set_selection(0,current.text.size()));good(tree->input({.kind=WidgetInputKind::text,.text=std::move(text)}));};
    click(40,10);key(WidgetKey::enter);
    if(!list->editor()){
        auto failure=take(list->failure());throw std::runtime_error("Enter did not open the selected list row editor; selected="+
            take(input.controller->selection())+" rowfocused="+std::to_string(take(take(list->row("a")).interaction()).focused)+
            " failure="+(failure?failure->message:std::string("none")));
    }
    auto field=take(list->editor());good(tree->input({.kind=WidgetInputKind::composition,.text="ㅎ"}));
    const auto before_palette=take(field.snapshot());input.palette.focus=lvui::figma_defaults::accent_purple_default;
    good(list->set_options(input));const auto after_palette=take(field.snapshot());
    require(before_palette.marked&&after_palette.marked==before_palette.marked&&after_palette.text==before_palette.text&&
        after_palette.selection==before_palette.selection&&take(field.root().snapshot()).focused&&take(source->snapshot())->revision==0,
        "Changing a list palette disturbed its active composition or data");
    const auto drawing=take(tree->drawing_snapshot({200,100}));const auto bounds=take(field.root().snapshot()).bounds;
    const auto expected=lvui::figma_defaults::accent_purple_default;
    const auto linear=[](double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);};
    const auto matches=[&](const auto& panels){return std::ranges::any_of(panels,[&](const auto& panel){
        const auto& b=panel.bounds;const auto& c=panel.top;return b.x>=bounds.x-.01&&b.y>=bounds.y-.01&&
            b.x+b.width<=bounds.x+bounds.width+.01&&b.y+b.height<=bounds.y+bounds.height+.01&&
            std::abs(c.red-linear(expected.red))<1e-5&&std::abs(c.green-linear(expected.green))<1e-5&&
            std::abs(c.blue-linear(expected.blue))<1e-5&&std::abs(c.alpha-expected.alpha)<1e-5;});};
    require(matches(drawing.panels)||std::ranges::any_of(drawing.overlays,[&](const auto& layer){return matches(layer.panels);}),
        "Active list editor did not inherit its new focus color");
    good(tree->input({.kind=WidgetInputKind::text,.text="x"}));
    replace("forbidden");require(!drafts.empty()&&drafts.back()=="forbidden"&&latest&&latest->entry.key=="a"&&latest->index==0&&
        take(source->snapshot())->revision==0,"List typing lost its row context or edited the source");
    key(WidgetKey::enter);require(rejected==std::vector<std::string>{"forbidden"}&&take(list->editor().value().snapshot()).text=="forbidden"&&
        take(source->snapshot())->revision==0,"A rejected list edit lost its draft");
    click(40,30);require(list->editor()&&take(list->editor().value().snapshot()).text=="forbidden"&&
        take(input.controller->selection())=="a"&&take(source->snapshot())->revision==0,
        "Changing rows after a rejected focus-loss commit discarded its draft or selection");
    replace("Renamed");key(WidgetKey::enter);require(submitted==std::vector<std::string>{"Renamed"}&&!list->editor()&&
        take(source->snapshot())->entries[0].label=="Renamed"&&take(source->snapshot())->revision==1,"A list rename did not commit exactly once");
    good(list->begin_edit());good(take(list->editor()).set_text("Program"));good(list->commit_edit());
    require(take(source->snapshot())->revision==2&&take(source->snapshot())->entries[0].label=="Program"&&
        submitted==std::vector<std::string>{"Renamed","Program"},"Programmatic list editor text was not committed");
    key(WidgetKey::enter);replace("Pending");auto next=take(source->snapshot())->entries[0];next.label="Newer";
    input.source_revision=std::int64_t(take(source->apply({.revision=2,.kind=lvui::ListEditKind::replace,.key="a",.item=next})));
    good(list->set_options(input));require(take(list->editor().value().snapshot()).text=="Pending","Refreshing a list discarded its draft");
    key(WidgetKey::enter);require(rejected.back()=="Pending"&&take(source->snapshot())->entries[0].label=="Newer"&&
        take(list->editor().value().snapshot()).text=="Pending","A stale list edit overwrote newer data or lost its draft");
    key(WidgetKey::escape);require(!list->editor()&&!take(list->failure()),"Escape did not cancel the list edit and clear its error");
    good(list->begin_edit());replace("Focus saved");click(40,30);
    require(!list->editor()&&take(source->snapshot())->revision==4&&take(source->snapshot())->entries[0].label=="Focus saved"&&
        submitted==std::vector<std::string>{"Renamed","Program","Focus saved"},"Losing focus did not submit the list edit");
    require(take(input.controller->selection())=="locked","Committing an edit during a row click lost the target selection");
    require(!list->begin_edit(),"A read-only list row entered edit mode");
    click(40,10);
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=10,.buttons=1,.native_click_count=2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=10,.native_click_count=2}));
    require(bool(list->editor()),"Double-click did not open the row editor");key(WidgetKey::escape);
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=10,.timestamp=1.0,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=10,.timestamp=1.01}));
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=40,.y=10,.timestamp=1.2,.buttons=1}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=40,.y=10,.timestamp=1.21}));
    require(bool(list->editor()),"Timed double-click fallback did not open the row editor");
    replace("Unsaved toggle");input.editable=false;good(list->set_options(input));
    require(!list->editor()&&take(source->snapshot())->revision==4,"Disabling editing retained or committed its draft");
    input.editable=true;good(list->set_options(input));
    click(40,10);good(list->begin_edit());replace("Unsaved");good(list->root().set_scroll_offset(400));
    require(!list->editor()&&take(source->snapshot())->revision==4,"Scrolling away committed or retained an offscreen editor");
    good(list->root().set_scroll_offset(0));click(40,10);
    auto retiring=std::make_shared<RetiringEditor>();retiring->source=source;retiring->host=host;input.editor=retiring;good(list->set_options(input));
    good(list->begin_edit());replace("Retired");key(WidgetKey::enter);
    require(take(source->snapshot())->revision==5&&submitted==std::vector<std::string>{"Renamed","Program","Focus saved"}&&!list->editor(),
        "A retiring list editor delivered a stale callback or retained its view");
    tree->close();embedded_controls();shared_controller_editing();
}
