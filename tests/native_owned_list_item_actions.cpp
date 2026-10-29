#include <LVUI/NativeListItemComposite.h>
#include <iiGUI/WidgetState.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
struct Command final:lvui::ListFooterCommand {std::function<void()> action;void invoke()const override{if(action)action();}};
}
int main(){
    using namespace lvui;using namespace ii::gui;
    native::ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::action_group;input.row.configuration.label="Project";
    input.primary_action.text="Open";input.secondary_action.text="Edit";input.more_menu.text="More";
    auto command=std::make_shared<Command>();unsigned invoked=0;command->action=[&]{++invoked;};input.primary_action.command=command;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{450,90}));std::optional<native::NativeListItemComposite> item;
    std::vector<std::string> actions;unsigned clicks=0;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto column=builder.vstack({},"column",{.spacing=0,.alignment=StackAlignment::leading});if(!column)return std::unexpected(column.error());
        auto value=native::NativeListItemComposite::create(builder,*column,"actions",input,
            {.activated=[&]{++clicks;},.action_triggered=[&](std::string name,const ListFooterSlot& slot){actions.push_back(name+":"+slot.text);}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    require(take(item->root().snapshot()).size.width==400&&take(item->root().snapshot()).size.height==44,"Action group default size changed");
    good(take(item->action(0)).activate());good(take(item->action(1)).activate());good(take(item->action(2)).activate());
    require(invoked==1&&clicks==0&&actions==std::vector<std::string>({"primary:Open","secondary:Edit","more:More"}),
            "Action slots did not invoke their own commands and events");
    command->action=[] {throw std::runtime_error("Command failed");};
    good(take(item->action(0)).activate());require(take(item->failure()).has_value()&&actions.size()==3,"Item did not expose a failed action command");
    command->action=[&]{++invoked;};
    auto next=take(item->options());next.show_secondary_action=false;good(item->set_options(next));
    require(!take(item->failure()),"Successful action reconfiguration did not clear the failure");
    require(!take(take(item->action(1)).interaction()).visible,"Hidden secondary action remained visible");
    next.row.configuration.variant=ListItemVariant::action;next.show_secondary_action=true;good(item->set_options(next));
    require(take(take(item->action(0)).interaction()).visible&&!take(take(item->action(1)).interaction()).visible&&
            !take(take(item->action(2)).interaction()).visible,"Single action row displayed extra actions");
    auto bad=next;bad.primary_action.icon_name="missing-icon";
    require(!item->set_options(bad)&&take(item->options())==next,"Invalid action icon replaced the previous item");
    good(host.clear_content());
    input.row.configuration.variant=ListItemVariant::action;unsigned stale=0;command->action=[&]{good(host.clear_content());};
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=native::NativeListItemComposite::create(builder,{},"retiring",input,
            {.action_triggered=[&](std::string,const ListFooterSlot&){++stale;}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    good(take(item->action(0)).activate());require(stale==0&&!item->root().snapshot(),"A removed item received a stale action callback");tree->close();
}
