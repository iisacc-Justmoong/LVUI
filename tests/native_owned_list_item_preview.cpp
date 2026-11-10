#include <LVUI/NativeListItemComposite.h>
#include "list_item_composite_test_binding.h"
#include <iiGUI/Image.h>
#include <iiGUI/WidgetState.h>

#include <functional>
#include <cstdint>
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
bool contains_image(const ii::gui::CanvasFrame& frame,const std::shared_ptr<const ii::gui::RasterImage>& image){
    for(const auto& run:frame.images)if(run.image==image&&run.bounds.width==48&&run.bounds.height==24)return true;
    for(const auto& layer:frame.overlays)for(const auto& run:layer.images)if(run.image==image&&run.bounds.width==48&&run.bounds.height==24)return true;
    return false;
}
bool contains_thin_image(const ii::gui::CanvasFrame& frame,const std::shared_ptr<const ii::gui::RasterImage>& image){
    for(const auto& layer:frame.overlays)for(const auto& run:layer.images)
        if(run.image==image&&run.bounds.height<1&&run.radius<=run.bounds.height/2)return true;
    return false;
}
bool has_image(const ii::gui::CanvasFrame& frame){
    if(!frame.images.empty())return true;for(const auto& layer:frame.overlays)if(!layer.images.empty())return true;return false;
}
struct Command final:lvui::ListFooterCommand{int* count;std::function<void()> action;void invoke()const override{++*count;if(action)action();}};
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::resource;
    input.row.configuration.label="Reference";input.row.configuration.description="Project asset";
    input.date_text="Tomorrow";input.metadata1="Studio";input.metadata2="Ready";input.status_text="Shared";
    input.preview=take(rgba_image(2,1,{255,0,0,255,0,255,0,255}));
    input.primary_action.text="Open";input.secondary_action.text="Reveal";input.more_menu.text="More";
    input.more_items={{"Duplicate",true},{"Locked",false}};int commands=0;auto command=std::make_shared<Command>();command->count=&commands;input.more_menu.command=command;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,220}));std::optional<native::NativeListItemComposite> item;
    std::vector<std::string> events;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"item",input,{
            .edited=[&](std::string name,const ListItemValue&){events.push_back("edit:"+name);},
            .action_triggered=[&](std::string name,const ListFooterSlot& slot){events.push_back(name+":"+slot.text);},
            .menu_item_triggered=[&](int,const ListItemOption& option){events.push_back("menu:"+option.text);},
            .segment_index_changed=[&](int value){events.push_back("segment:"+std::to_string(value));}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    require(take(item->root().snapshot()).size.height==129,"Resource default extent changed");
    auto frame=take(tree->drawing_snapshot({500,220}));
    for(auto text:{"Reference","Project asset","Tomorrow","Studio","Ready","Shared"})require(contains(frame,text),"Resource omitted visible content");
    require(contains_image(frame,input.preview),"Resource preview was not fitted to its frame");
    auto more=take(item->action(2));good(more.activate());require(commands==1&&take(take(item->action_menu()).snapshot()).opened,"Resource menu did not open from its header");
    good(take(item->menu_item(0)).activate());good(take(item->action(1)).activate());good(take(item->action(0)).activate());
    require(events==std::vector<std::string>({"more:More","menu:Duplicate","secondary:Reveal","primary:Open"}),"Resource menu or actions lost an event");
    require(take(more.snapshot()).bounds.y<take(take(item->action(0)).snapshot()).bounds.y&&
        take(take(item->action(1)).snapshot()).bounds.x<take(take(item->action(0)).snapshot()).bounds.x,"Resource action order changed");
    good(more.activate());require(take(take(item->action_menu()).snapshot()).opened,"Resource menu did not reopen");
    auto next=take(item->options());next.row.configuration.variant=ListItemVariant::media;next.row.configuration.label="Audio";
    next.row.configuration.description="Voice track";next.duration_text="03:42";next.media_detail="WAV · 48 kHz";
    next.primary_action.text="Play";next.secondary_action.text="Stop";next.segments={{"Original",true,{}},{"Edited",true,{}}};
    good(item->set_options(next));require(!take(take(item->action_menu()).snapshot()).opened&&take(item->root().snapshot()).size.height==113,
        "Switching to media retained the resource menu or extent");
    frame=take(tree->drawing_snapshot({500,220}));
    for(auto text:{"Audio","Voice track","03:42","WAV · 48 kHz","Shared"})require(contains(frame,text),"Media omitted visible content");
    require(contains_image(frame,input.preview),"Media lost the preview");
    auto invalid=take(item->options());invalid.preview.reset();invalid.preview_icon_name="missing-preview-icon";
    require(!item->set_options(invalid)&&take(item->options()).preview==input.preview,"A missing preview icon replaced the current item");
    auto fallback=take(item->options());fallback.preview.reset();fallback.preview_icon_name="nodesfolder";good(item->set_options(fallback));
    require(has_image(take(tree->drawing_snapshot({500,220}))),"The named preview icon did not resolve");
    auto segments=take(item->segments());good(take(segments.segment(1)).activate());good(take(item->action(1)).activate());good(take(item->action(0)).activate());
    require(take(item->segment_index())==1&&events[events.size()-4]=="segment:1"&&events[events.size()-3]=="edit:segmentIndex"&&
        events[events.size()-2]=="secondary:Stop"&&events.back()=="primary:Play","Media segments or actions lost an event");
    next=take(item->options());next.preview=take(rgba_image(100,1,std::vector<std::uint8_t>(400,255)));good(item->set_options(next));
    require(contains_thin_image(take(tree->drawing_snapshot({500,220})),next.preview),"A thin preview produced an invalid rounded image");
    next.show_preview=false;next.show_segments=false;good(item->set_options(next));
    require(!take(segments.root().interaction()).visible&&!contains_thin_image(take(tree->drawing_snapshot({500,220})),next.preview),
        "Media failed to hide its preview or segments");
    next.preview.reset();next.preview_icon_name="missing-preview-icon";good(item->set_options(next));
    require(!take(item->failure()),"A hidden preview tried to resolve its icon");
    good(host.clear_content());require(!item->set_options(input),"Retired preview item accepted an update");
    std::optional<native::NativeListItemComposite> retiring;command->action=[&]{good(host.clear_content());};const auto before=events.size();
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"retiring",input,{
            .action_triggered=[&](std::string,const ListFooterSlot&){events.push_back("stale");}});
        if(!value)return std::unexpected(value.error());retiring=*value;return {};
    }));
    good(take(retiring->action(2)).activate());
    require(events.size()==before&&!retiring->root().snapshot(),"A retired resource menu emitted a stale action");tree->close();
}
