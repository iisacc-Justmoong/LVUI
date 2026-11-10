#include <LVUI/NativeListItemComposite.h>
#include "list_item_composite_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <memory>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
bool contains(const ii::gui::CanvasFrame& frame,std::string_view text){
    for(const auto& run:frame.texts)if(run.text==text)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.texts)if(run.text==text)return true;return false;
}
bool has_color(const ii::gui::CanvasFrame& frame,const ii::gui::WidgetSnapshot& widget,lvui::StyleColor color){
    const auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};
    const auto matches=[&](const auto& panels){for(const auto& panel:panels){const auto& b=panel.bounds;const auto& w=widget.bounds;const auto& c=panel.top;
        if(b.width>0&&b.height>0&&b.x>=w.x-.01&&b.y>=w.y-.01&&b.x+b.width<=w.x+w.width+.01&&b.y+b.height<=w.y+w.height+.01&&
           std::abs(c.red-linear(color.red))<1e-5&&std::abs(c.green-linear(color.green))<1e-5&&
           std::abs(c.blue-linear(color.blue))<1e-5&&std::abs(c.alpha-color.alpha)<1e-5)return true;}return false;};
    if(matches(frame.panels))return true;for(const auto& layer:frame.overlays)if(matches(layer.panels))return true;return false;
}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::form;
    input.row.configuration.label="Publish";input.row.configuration.description="Information";
    input.field_label1="Title";input.field_label2="Notes";input.input_text="A😀Z";input.input_text2="Draft";
    input.selector.options={{"Private",true},{"Public",true}};input.primary_action.text="Save";input.secondary_action.text="Cancel";
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,230}));std::optional<native::NativeListItemComposite> item;
    std::vector<std::string> events;std::vector<ListItemActionValues> snapshots;
    std::vector<std::pair<std::string,ListItemActionPayload>> unified;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"form",input,{
            .checked_changed=[&](bool value){events.push_back(value?"checked:true":"checked:false");},
            .edited=[&](std::string field,const ListItemValue&){events.push_back("edit:"+field);},
            .action_triggered=[&](std::string name,const ListFooterSlot& slot){events.push_back(name+":"+slot.text);},
            .selector_index_changed=[&](int value){events.push_back("category:"+std::to_string(value));},
            .input_text_changed=[&](std::string value){events.push_back("title:"+value);auto next=take(item->options());next.input_text=value;good(item->set_options(next));},
            .input_text2_changed=[&](std::string value){events.push_back("notes:"+value);auto next=take(item->options());next.input_text2=value;good(item->set_options(next));},
            .input_submitted2=[&](std::string value){events.push_back("submit:"+value);},
            .action_values_triggered=[&](std::string name,const ListItemActionValues& value){events.push_back("values:"+name);snapshots.push_back(value);},
            .action_event=[&](std::string name,const ListItemActionPayload& payload){unified.emplace_back(std::move(name),payload);}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    require(take(item->root().snapshot()).size.height==170&&take(item->checked()),"Form defaults or extent changed");
    auto frame=take(tree->drawing_snapshot({500,230}));
    for(auto text:{"Publish","Information","Title","Notes","Draft"})require(contains(frame,text),"Form omitted visible content");
    auto first=take(item->input_field()),second=take(item->input_field2());
    require(take(first.root().snapshot()).accessibility_name=="Title"&&take(second.root().snapshot()).accessibility_name=="Notes",
            "Form fields lost their independent accessibility names");
    good(first.root().focus());good(first.set_selection(1,3));good(tree->input({.kind=WidgetInputKind::composition,.text="ㅎ"}));
    auto next=take(item->options());next.input_text2="Memo";next.accent=WindowAccentPalette{.primary=figma_defaults::accent_purple_default};
    good(item->set_options(next));
    require(take(first.snapshot()).marked.has_value()&&take(second.snapshot()).text=="Memo"&&
            take(first.root().snapshot()).focused&&take(first.options()).palette.focus==figma_defaults::accent_purple_default,
            "Updating accent and the second field interrupted the first composition");
    auto invalid_accent=next;invalid_accent.accent->primary.red=-1;
    require(!item->set_options(invalid_accent)&&take(item->options())==next&&take(first.snapshot()).marked.has_value(),
            "Invalid composite accent changed committed options or interrupted composition");
    good(tree->input({.kind=WidgetInputKind::text,.text="한"}));
    require(take(item->input_text())=="A한Z"&&take(item->input_text2())=="Memo","First form field did not commit independently");
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    good(second.root().focus());good(second.set_selection(0,4));good(tree->input({.kind=WidgetInputKind::text,.text="Idea"}));
    good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    auto selector=take(item->selector());good(take(selector.control()).activate());
    auto menu=take(selector.menu()),selected=take(selector.option(0)),option=take(selector.option(1));good(selected.focus());
    next=take(item->options());next.accent=WindowAccentPalette{.primary=figma_defaults::accent_green_default};good(item->set_options(next));
    require(take(menu.snapshot()).opened&&take(selected.snapshot()).focused&&take(option.snapshot()).id==take(take(selector.option(1)).snapshot()).id&&
            take(selector.options()).accent==next.accent&&
            has_color(take(tree->drawing_snapshot({500,230})),take(take(selector.option(0)).snapshot()),figma_defaults::accent_green_default),
            "Changing accent replaced or failed to restyle an open selector");
    good(option.activate());
    good(item->toggle().activate());good(take(item->action(1)).activate());good(take(item->action(0)).activate());
    require(take(item->input_text2())=="Idea"&&take(item->selector_index())==1&&!take(item->checked())&&
        events==std::vector<std::string>({"title:A한Z","edit:inputText1","notes:Idea","edit:inputText2","submit:Idea",
            "category:1","edit:selectorIndex","checked:false","edit:checked","secondary:Cancel","values:secondary","primary:Save","values:primary"}),
        "Form fields, actions, or edit events changed");
    require(snapshots.size()==2&&snapshots.back()==take(item->action_values())&&snapshots.back().input_text1=="A한Z"&&
        snapshots.back().input_text2=="Idea"&&!snapshots.back().checked&&snapshots.back().selector_index==1,
        "Form action did not capture the current independent values");
    require(unified.size()==4&&unified[0].first=="inputSubmitted"&&
        std::get<ListItemInputSubmission>(unified[0].second)==ListItemInputSubmission{"inputText1","A한Z"}&&
        unified[1].first=="inputSubmitted"&&std::get<ListItemInputSubmission>(unified[1].second)==ListItemInputSubmission{"inputText2","Idea"}&&
        unified[2].first=="secondary"&&unified[3].first=="primary"&&
        std::get<ListItemActionValues>(unified[3].second)==snapshots.back(),
        "Unified action event lost a typed field submission or value snapshot");
    bool wrong_thread=false;std::thread worker([&]{auto values=item->action_values();wrong_thread=!values&&values.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(wrong_thread,"Action values accepted a foreign owner thread");
    require(take(take(item->action(1)).snapshot()).bounds.x<take(take(item->action(0)).snapshot()).bounds.x,
            "Form did not place Cancel before Save");
    next=take(item->options());next.input2_read_only=true;good(item->set_options(next));good(second.root().focus());
    good(tree->input({.kind=WidgetInputKind::text,.text="X"}));require(take(item->input_text2())=="Idea","Read-only second field changed");
    next.show_input=false;next.show_input2=false;good(item->set_options(next));
    require(take(item->root().snapshot()).size.height==132&&!take(first.root().interaction()).visible&&!take(second.root().interaction()).visible,
        "Form did not hide its two independent fields");
    good(host.clear_content());require(!item->set_options(input)&&!item->action_values(),"Retired form accepted an update or read");
    unsigned stale=0;good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"retiring",input,{
            .edited=[&](std::string,const ListItemValue&){++stale;},
            .input_text2_changed=[&](std::string){good(host.clear_content());}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    auto retiring=take(item->input_field2());good(retiring.root().focus());good(tree->input({.kind=WidgetInputKind::text,.text="!"}));
    require(stale==0&&!item->root().snapshot(),"A retired form emitted a stale second-field edit");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"retiring-action",input,{
            .action_triggered=[&](std::string,const ListFooterSlot&){good(host.clear_content());},
            .action_values_triggered=[&](std::string,const ListItemActionValues&){++stale;},
            .action_event=[&](std::string,const ListItemActionPayload&){++stale;}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    good(take(item->action(0)).activate());require(stale==0&&!item->root().snapshot(),"A retired action emitted stale values");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"changed-action",input,{
            .action_triggered=[&](std::string,const ListFooterSlot&){auto next=take(item->options());next.input_text="Changed";good(item->set_options(next));},
            .action_values_triggered=[&](std::string,const ListItemActionValues&){++stale;},
            .action_event=[&](std::string,const ListItemActionPayload&){++stale;}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    good(take(item->action(0)).activate());
    require(stale==0&&take(item->input_text())=="Changed","An action emitted an outdated value snapshot after a reentrant update");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"changed-values",input,{
            .action_values_triggered=[&](std::string,const ListItemActionValues&){auto next=take(item->options());next.input_text2="Updated";good(item->set_options(next));},
            .action_event=[&](std::string,const ListItemActionPayload&){++stale;}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    good(take(item->action(0)).activate());
    require(stale==0&&take(item->input_text2())=="Updated","A unified action event survived a reentrant value callback");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"retiring-submission",input,{
            .input_submitted2=[&](std::string){good(host.clear_content());},
            .action_event=[&](std::string,const ListItemActionPayload&){++stale;}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    auto submitted=take(item->input_field2());good(submitted.root().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::enter}));
    require(stale==0&&!item->root().snapshot(),"A retired submission emitted an outdated unified event");tree->close();
}
