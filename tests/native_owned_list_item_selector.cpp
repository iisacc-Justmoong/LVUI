#include <LVUI/NativeListItemSelector.h>
#include <iiGUI/WidgetState.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
}
int main(){
    using namespace lvui;using namespace ii::gui;
    require(native::list_item_selector(std::vector<std::string>{"One","Two"}).options==std::vector<ListItemOption>({{"One",true},{"Two",true}}),
            "String options did not convert to an independent selector configuration");
    native::ListItemSelectorInput input;input.configuration.options={{"Draft",true},{"Unavailable",false},{"Published",true}};
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{300,180}));std::optional<native::NativeListItemSelector> selector;
    std::vector<int> changes;bool replace_during_change=false;
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=native::NativeListItemSelector::create(builder,*stack,"selector",input,[&](int index){changes.push_back(index);
            auto next=take(selector->options());next.configuration.selected_index=index;
            if(replace_during_change){next.configuration.options={{"Replacement",true}};next.configuration.selected_index=0;}
            good(selector->set_options(next));});
        if(!value)return std::unexpected(value.error());selector=*value;return {};
    }));
    const auto initial=take(tree->snapshots());
    require(std::ranges::any_of(initial,[](const auto& node){return node.id.ends_with("/control");}),"Selector did not mount after publication");
    auto control=take(selector->control());good(control.focus());
    const auto key=[&](WidgetKey value){good(tree->input({.kind=WidgetInputKind::key_down,.key=value}));};
    key(WidgetKey::down);key(WidgetKey::down);
    require(changes==std::vector<int>{2}&&take(selector->selected_index())==2&&take(control.action_options()).text=="Published",
            "Selector keyboard navigation, disabled choices or reactive echo failed");
    key(WidgetKey::up);require(changes==std::vector<int>({2,0})&&take(control.action_options()).text=="Draft","Selector did not navigate backwards");
    good(control.activate());auto menu=take(selector->menu());require(take(menu.snapshot()).opened,"Selector did not open its menu");
    good(take(selector->option(2)).activate());
    require(changes==std::vector<int>({2,0,2})&&!take(menu.snapshot()).opened,"Selector did not choose an option and close its menu");
    auto next=take(selector->options());next.configuration.options={{"New",true},{"Other",true}};next.configuration.selected_index=0;
    good(selector->set_options(next));auto updated=take(selector->control());
    require(!control.snapshot()&&take(updated.action_options()).text=="New","Changing selector options did not replace the old menu generation");
    good(updated.activate());auto replacement_menu=take(selector->menu());require(take(replacement_menu.snapshot()).opened,"Replacement selector menu did not open");
    next.configuration.enabled=false;good(selector->set_options(next));good(updated.activate());
    require(changes.size()==3&&!take(replacement_menu.snapshot()).opened,"Disabled selector changed or left its menu open");
    next.configuration.enabled=true;good(selector->set_options(next));good(updated.focus());replace_during_change=true;key(WidgetKey::down);
    auto replaced=take(selector->control());require(changes==std::vector<int>({2,0,2,1})&&!updated.snapshot()&&
            take(replaced.action_options()).text=="Replacement","A reentrant option replacement lost the selection or retained the old generation");
    next=take(selector->options());auto invalid=next;invalid.configuration.selected_index=4;
    require(!selector->set_options(invalid)&&take(selector->options())==next,"Invalid selector update replaced the last valid configuration");
    bool wrong_thread=false;std::thread worker([&]{auto result=selector->set_options(next);wrong_thread=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;});worker.join();
    require(wrong_thread,"Selector accepted a foreign owner thread");
    good(host.clear_content());require(!selector->set_options(input),"Retired selector accepted changes");tree->close();
}
