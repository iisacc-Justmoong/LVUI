#include <LVUI/NativeListItemComposite.h>
#include "list_item_composite_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool contains(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& run:frame.texts)if(run.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return true;return false;
}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::detail_quantity;
    input.row.configuration.label="Export";input.row.configuration.description="Output dimensions";
    input.selector.options={{"PNG",true},{"JPEG",true}};input.unit_selector.options={{"px",true},{"cm",true}};
    input.quantity=2;input.minimum_quantity=1;input.maximum_quantity=3;input.primary_action.text="Apply";
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,220}));std::optional<native::NativeListItemComposite> item;
    std::vector<std::string> events;std::vector<ListItemActionValues> snapshots;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"item",input,{
            .checked_changed=[&](bool value){events.push_back(value?"checked:true":"checked:false");},
            .edited=[&](std::string name,const ListItemValue&){events.push_back("edit:"+name);},
            .action_triggered=[&](std::string name,const ListFooterSlot& slot){events.push_back(name+":"+slot.text);},
            .quantity_changed=[&](double value){events.push_back("quantity:"+std::to_string(int(value)));},
            .selector_index_changed=[&](int value){events.push_back("selector:"+std::to_string(value));},
            .unit_index_changed=[&](int value){events.push_back("unit:"+std::to_string(value));},
            .segment_index_changed=[&](int value){events.push_back("segment:"+std::to_string(value));},
            .action_values_triggered=[&](std::string,const ListItemActionValues& value){snapshots.push_back(value);}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    require(take(item->root().snapshot()).size.height==118&&take(item->checked())&&take(item->quantity())==2,
            "Detail quantity defaults changed");
    auto frame=take(tree->drawing_snapshot({500,220}));
    for(auto text:{"Export","Output dimensions","Quantity label","Apply to all"})require(contains(frame,text),"Detail quantity omitted visible text");
    auto selector=take(item->selector()),unit=take(item->unit_selector());
    require(take(take(unit.control()).action_options()).accessibility_name=="Units"&&
            take(item->stepper().action_options()).accessibility_name=="Quantity label","Detail quantity accessibility names changed");
    good(take(selector.control()).activate());good(take(selector.option(1)).activate());
    good(take(unit.control()).activate());good(take(unit.option(1)).activate());
    good(item->stepper().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::up}));
    good(item->toggle().activate());good(take(item->action(0)).activate());
    require(take(item->selector_index())==1&&take(item->unit_index())==1&&take(item->quantity())==3&&!take(item->checked())&&
        events==std::vector<std::string>({"selector:1","edit:selectorIndex","unit:1","edit:unitIndex","quantity:3","edit:quantity",
            "checked:false","edit:checked","primary:Apply"}),"Detail quantity controls lost an event or value");
    require(snapshots.size()==1&&snapshots.back().quantity==3&&snapshots.back().selector_index==1&&
        snapshots.back().unit_index==1&&!snapshots.back().checked,"Quantity action values did not capture independent controls");
    auto directional=take(item->options());directional.quantity=2.5;directional.stepper_arrow=lvui::StepperArrow::down;good(item->set_options(directional));
    good(item->stepper().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::up}));
    require(take(item->quantity())==2.5,"A down-only stepper accepted an increment");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));require(take(item->quantity())==1.5,"A down-only stepper lost its decrement");
    directional.stepper_tone=lvui::ButtonTone::disabled;good(item->set_options(directional));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::down}));require(take(item->quantity())==1.5,"A disabled stepper changed its value");
    auto bad=take(item->options());bad.unit_index=9;require(!item->set_options(bad)&&take(item->unit_index())==1,"Invalid unit replaced the current state");
    auto next=take(item->options());next.row.configuration.variant=ListItemVariant::detail_settings;next.row.configuration.label="Behavior";
    next.row.configuration.description="Settings";next.primary_action.text="Apply";next.secondary_action.text="Reset";
    next.selector.options={{"Automatic",true},{"Manual",true}};next.selector_index=0;next.checked.reset();
    next.segments={{"Original",true,{}},{"Unavailable",false,{}},{"Edited",true,{}}};next.segment_index=0;
    good(item->set_options(next));require(take(item->root().snapshot()).size.height==159&&take(item->checked()),"Detail settings defaults changed");
    frame=take(tree->drawing_snapshot({500,220}));
    for(auto text:{"Behavior","Settings","Mode label","Scope label","Draft"})require(contains(frame,text),"Detail settings omitted visible text");
    auto segments=take(item->segments());auto first=take(segments.segment(0)),third=take(segments.segment(2));
    good(first.focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::right}));
    require(take(item->segment_index())==2&&take(third.snapshot()).checked&&!take(first.snapshot()).checked,
            "Detail settings segments did not skip disabled options");
    const auto reset=take(take(item->action(1)).snapshot()).bounds;
    good(tree->input({.kind=WidgetInputKind::pointer_down,.x=reset.x+reset.width/2,.y=reset.y+reset.height/2}));
    good(tree->input({.kind=WidgetInputKind::pointer_up,.x=reset.x+reset.width/2,.y=reset.y+reset.height/2}));
    good(take(item->action(0)).activate());
    require(events[events.size()-4]=="segment:2"&&events[events.size()-3]=="edit:segmentIndex"&&
        events[events.size()-2]=="secondary:Reset"&&events.back()=="primary:Apply","Detail settings action order or segment events changed");
    require(snapshots.size()==3&&snapshots.back().segment_index==2&&snapshots.back().checked,
        "Settings action values did not capture the selected segment");
    require(take(take(item->action(1)).snapshot()).bounds.x<take(take(item->action(0)).snapshot()).bounds.x,
            "Detail settings did not place Reset before Apply");
    auto disabled=take(item->options());disabled.row.configuration.enabled=false;good(item->set_options(disabled));
    const auto count=events.size();good(third.activate());good(take(item->action(0)).activate());
    require(events.size()==count,"A disabled detail item accepted a segment or action");
    disabled.row.configuration.enabled=true;good(item->set_options(disabled));
    next=take(item->options());next.show_segments=false;next.show_selector=false;next.show_toggle=false;good(item->set_options(next));
    require(!take(segments.root().interaction()).visible&&!take(item->toggle().interaction()).visible,"Detail settings ignored hidden controls");
    good(host.clear_content());require(!item->set_options(input),"Retired detail controls accepted an update");tree->close();
}
