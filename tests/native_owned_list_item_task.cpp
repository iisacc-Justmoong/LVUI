#include <LVUI/NativeListItemComposite.h>
#include "list_item_composite_test_binding.h"
#include <iiGUI/WidgetState.h>

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
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::task;
    input.row.configuration.label="Retouch";input.row.configuration.description="Remove blemishes";
    input.date_text="Tomorrow";input.metadata1="Studio";input.metadata2="Open";input.quantity_label="Hours";
    input.selector.options={{"Low",true},{"High",true}};input.quantity=2;input.maximum_quantity=3;
    input.primary_action.text="Done";input.secondary_action.text="Assign";
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,220}));std::optional<native::NativeListItemComposite> item;
    std::vector<std::string> events;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemComposite::create(builder,*stack,"task",input,{
            .checked_changed=[&](bool checked){events.push_back(checked?"checked:true":"checked:false");},
            .edited=[&](std::string name,const ListItemValue&){events.push_back("edit:"+name);},
            .action_triggered=[&](std::string name,const ListFooterSlot& slot){events.push_back(name+":"+slot.text);},
            .quantity_changed=[&](double value){events.push_back("quantity:"+std::to_string(int(value)));},
            .selector_index_changed=[&](int value){events.push_back("priority:"+std::to_string(value));}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    require(take(item->root().snapshot()).size.height==135&&!take(item->checked())&&take(item->quantity())==2,
            "Task defaults or extent changed");
    const auto frame=take(tree->drawing_snapshot({500,220}));
    for(auto text:{"Retouch","Remove blemishes","Tomorrow","Studio","Open","Hours"})
        require(contains(frame,text),"Task omitted a visible section");
    const auto description=take(item->row().action_options()).accessibility_description;
    require(description.find("Tomorrow")!=std::string::npos&&description.find("Hours")!=std::string::npos,
            "Task accessibility omitted its metadata or quantity");
    good(item->checkbox().activate());auto selector=take(item->selector());good(take(selector.control()).activate());good(take(selector.option(1)).activate());
    good(item->stepper().focus());good(tree->input({.kind=WidgetInputKind::key_down,.key=WidgetKey::up}));
    good(take(item->action(1)).activate());good(take(item->action(0)).activate());
    require(take(item->checked())&&take(item->selector_index())==1&&take(item->quantity())==3&&
        events==std::vector<std::string>({"checked:true","edit:checked","priority:1","edit:selectorIndex",
            "quantity:3","edit:quantity","secondary:Assign","primary:Done"}),"Task values or callbacks changed");
    require(take(take(item->action(1)).snapshot()).bounds.x<take(take(item->action(0)).snapshot()).bounds.x,
            "Task did not place Assign before Done");
    auto changed=take(item->options());changed.show_metadata=false;changed.row.configuration.show_description=false;
    changed.show_selection=false;changed.show_selector=false;good(item->set_options(changed));
    require(take(item->root().snapshot()).size.height==71,"Task retained the height of hidden sections and header controls");
    require(!take(item->checkbox().interaction()).visible&&!take(selector.root().interaction()).visible,
            "Task failed to hide its selection or selector control");
    changed.row.configuration.enabled=false;good(item->set_options(changed));const auto count=events.size();
    good(take(item->action(0)).activate());good(item->checkbox().activate());
    require(events.size()==count,"A disabled task accepted an action");
    good(host.clear_content());require(!item->set_options(input),"A retired task accepted an update");tree->close();
}
