#include <LVUI/NativeListItemComposite.h>
#include "list_item_composite_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool contains(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& run:frame.texts)if(run.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return true;
    return false;
}
std::size_t images(const ii::gui::CanvasFrame& frame){
    auto count=frame.images.size();for(const auto& layer:frame.overlays)count+=layer.images.size();return count;
}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::detail_actions;
    input.row.configuration.label="Project";input.row.configuration.description="First line\nSecond line";
    input.date_text="2026-11-02";input.metadata1="Illustration";input.metadata2="Ready";input.status_text="Draft";
    input.primary_action.text="Open";input.secondary_action.text="Edit";
    input.more_items={{"Rename",true},{"Unavailable",false},{"Delete",true}};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{450,160}));std::optional<native::NativeListItemComposite> item;
    std::vector<std::string> actions,menu_events;std::vector<std::pair<std::string,ListItemActionPayload>> unified;unsigned clicks=0;bool replace=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=native::NativeListItemComposite::create(builder,*column,"item",input,
            {.activated=[&]{++clicks;},.action_triggered=[&](std::string name,const ListFooterSlot&){actions.push_back(name);},
             .menu_item_triggered=[&](int index,const ListItemOption& option){menu_events.push_back(std::to_string(index)+":"+option.text);
                 if(replace){auto next=take(item->options());next.more_items={{"Replacement",true}};good(item->set_options(next));}},
             .action_event=[&](std::string name,const ListItemActionPayload& payload){unified.emplace_back(std::move(name),payload);}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    auto snapshot=take(item->root().snapshot());
    require(snapshot.size.width==400&&snapshot.size.height==120,"Compound item natural extent changed");
    auto frame=take(tree->drawing_snapshot({450,160}));
    for(auto text:{"Project","First line","Second line","2026-11-02","Illustration","Ready","Draft"})
        require(contains(frame,text),"Compound item omitted its visible content");
    require(images(frame)>=2,"Compound item did not draw its leading and bookmark icons");
    require(take(take(item->action(0)).snapshot()).bounds.width==72&&
            take(item->row().action_options()).accessibility_description=="First line\nSecond line · 2026-11-02 · Illustration · Ready · Draft",
            "Compound item lost its fixed action size or accessible description");
    const auto click=[&](float x,float y){good(tree->input({.kind=WidgetInputKind::pointer_down,.x=x,.y=y}));
        good(tree->input({.kind=WidgetInputKind::pointer_up,.x=x,.y=y}));};
    auto row_bounds=take(item->row().snapshot()).bounds;click(row_bounds.x+3,row_bounds.y+3);
    auto button_bounds=take(take(item->action(0)).snapshot()).bounds;click(button_bounds.x+button_bounds.width/2,button_bounds.y+button_bounds.height/2);
    require(clicks==1&&actions==std::vector<std::string>{"primary"},"Compound hit testing lost the row or action");
    clicks=0;actions.clear();
    good(item->row().activate());good(take(item->action(0)).activate());good(take(item->action(1)).activate());
    button_bounds=take(take(item->action(2)).snapshot()).bounds;click(button_bounds.x+button_bounds.width/2,button_bounds.y+button_bounds.height/2);
    require(clicks==1&&actions==std::vector<std::string>({"primary","secondary","more"}),"Compound controls lost their events");
    auto menu=take(item->action_menu());if(!take(menu.snapshot()).opened){auto failure=take(item->failure());
        throw std::runtime_error(failure?failure->message:"More action did not open its menu");}
    good(take(item->menu_item(1)).activate());require(menu_events.empty(),"Disabled menu entry activated");
    auto menu_bounds=take(take(item->menu_item(2)).snapshot()).bounds;click(menu_bounds.x+menu_bounds.width/2,menu_bounds.y+menu_bounds.height/2);
    require(menu_events==std::vector<std::string>{"2:Delete"}&&!take(menu.snapshot()).opened,"Menu action did not emit or close");
    require(unified.back().first=="menuItem"&&std::get<ListItemMenuSelection>(unified.back().second)==ListItemMenuSelection{2,{"Delete",true}},
            "Unified event did not retain the selected menu item and index");
    good(take(item->action(2)).activate());auto old_item=take(item->menu_item(0));replace=true;good(old_item.activate());replace=false;
    require(menu_events.back()=="0:Rename","Menu callback did not receive its original entry");
    require(unified.back().first=="menu","A reentrant menu replacement delivered a stale unified selection");
    good(take(item->action(2)).activate());auto replacement=take(item->action_menu());
    require(take(replacement.snapshot()).opened&&!old_item.snapshot(),"A changed menu retained the previous entry generation");
    good(take(item->menu_item(0)).activate());require(menu_events.back()=="0:Replacement","Replacement menu did not activate");
    good(take(item->action(2)).activate());auto current_item=take(item->menu_item(0));auto shown=take(item->options());
    auto hidden=shown;hidden.show_more_menu=false;good(item->set_options(hidden));
    require(!take(replacement.snapshot()).opened&&!take(take(item->action(2)).interaction()).visible,
            "Hiding the more action did not close its menu");
    const auto count=menu_events.size();require(!current_item.activate()&&menu_events.size()==count,"A hidden menu delivered a stale event");
    good(item->set_options(shown));
    auto next=take(item->options());next.show_metadata=false;next.show_bookmark=false;next.show_secondary_action=false;
    good(item->set_options(next));snapshot=take(item->root().snapshot());
    require(snapshot.size.height==97&&!take(take(item->action(1)).interaction()).visible,"Reactive compound visibility did not update");
    require(take(item->row().action_options()).accessibility_description=="First line\nSecond line · Draft",
            "Hidden metadata remained in the accessible description");
    frame=take(tree->drawing_snapshot({450,160}));
    require(!contains(frame,"Illustration")&&contains(frame,"Draft"),"Hidden metadata was still drawn");
    auto long_text=next;long_text.row.configuration.description=std::string(8191,'a')+"한글";
    long_text.metadata1=std::string(65536,'z');long_text.show_metadata=true;long_text.row.configuration.width=80;
    good(item->set_options(long_text));(void)take(tree->drawing_snapshot({450,160}));
    require(take(item->options())==long_text,"Bounded Unicode drawing changed the authored values");
    good(item->set_options(next));
    auto invalid=next;invalid.status_text=std::string("bad\xff",4);
    require(!item->set_options(invalid)&&take(item->options())==next,"Invalid content replaced the committed item");
    invalid=next;invalid.show_bookmark=true;invalid.bookmark_icon_name="missing-icon";
    require(!item->set_options(invalid)&&take(item->options())==next,"An unresolved bookmark replaced the committed item");
    auto compact=next;compact.row.configuration.show_description=false;good(item->set_options(compact));
    require(take(item->root().snapshot()).size.height==76&&take(item->row().action_options()).accessibility_description=="Draft",
            "Hiding the description did not update layout and accessibility");
    good(item->set_options(next));
    next.row.configuration.enabled=false;good(item->set_options(next));good(item->row().activate());good(take(item->action(0)).activate());
    require(clicks==1&&actions.size()==6,"Disabled compound item accepted an activation");
    good(host.clear_content());require(!item->set_options(input),"Retired compound item accepted an update");tree->close();
}
