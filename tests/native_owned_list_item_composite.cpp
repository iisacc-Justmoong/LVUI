#include <LVUI/NativeListItemComposite.h>
#include <iiGUI/WidgetState.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <variant>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::toggle;input.row.configuration.label="Camera";
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{350,100}));std::optional<native::NativeListItemComposite> item;
    std::vector<bool> changes;std::vector<std::string> fields;unsigned clicks=0;bool echo=true;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=native::NativeListItemComposite::create(builder,*column,"item",input,
            {.activated=[&]{++clicks;},.checked_changed=[&](bool value){changes.push_back(value);if(echo){auto next=take(item->options());next.checked=value;good(item->set_options(next));}},
             .edited=[&](std::string field,const ListItemValue& value){fields.push_back(field);require(std::holds_alternative<bool>(value),"Toggle event has the wrong value type");}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    require(take(item->root().snapshot()).size.width==280&&take(item->root().snapshot()).size.height==44&&take(item->checked())&&
            take(item->toggle().interaction()).visible&&!take(item->checkbox().interaction()).visible,"Toggle row defaults or layout changed");
    auto unused_selector=take(item->selector());require(!take(unused_selector.root().snapshot()).has_content,"A hidden selector mounted unnecessarily");
    auto renamed=take(item->options());renamed.row.configuration.label="Camera updated";good(item->set_options(renamed));
    require(!take(unused_selector.root().snapshot()).has_content,"A hidden selector mounted during an unrelated row update");
    good(item->toggle().activate());
    require(changes==std::vector<bool>{false}&&fields==std::vector<std::string>{"checked"}&&!take(item->checked()),
            "Toggle row lost its checked callbacks or reactive echo");
    good(item->row().activate());require(clicks==1,"List row activation was lost");
    auto next=take(item->options());next.toggle_enabled=false;good(item->set_options(next));good(item->toggle().activate());
    require(changes.size()==1,"Disabled toggle accepted an event");
    next.toggle_enabled=true;next.row.configuration.variant=ListItemVariant::checkable;next.checked=false;good(item->set_options(next));
    require(!take(item->toggle().interaction()).visible&&take(item->checkbox().interaction()).visible,"Checkable row did not switch its control");
    good(item->checkbox().activate());require(take(item->checked())&&changes.back()&&fields.size()==2,"Checkable row did not publish its value");
    bool wrong=false;std::thread worker([&]{auto value=item->set_options(input);wrong=!value&&value.error().code==ii::core::ErrorCode::wrong_executor;});
    worker.join();require(wrong,"Composite list item accepted a foreign-thread update");
    good(host.clear_content());require(!item->set_options(input),"Retired list item accepted an update");
    unsigned stale=0;echo=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"retiring",input,
            {.checked_changed=[&](bool){good(host.clear_content());},.edited=[&](std::string,const ListItemValue&){++stale;}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    good(item->toggle().activate());require(stale==0&&!item->root().snapshot(),"Retired item received a stale edit callback");tree->close();
}
