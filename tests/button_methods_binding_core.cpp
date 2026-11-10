#include <LVUI/NativeButtonMethods.h>
#include <memory>
#include <stdexcept>
#include <vector>
struct State{bool alive=true,enabled=true;};
struct Snapshot{bool enabled;};struct Interaction{bool alive,enabled,visible,input_blocked;};struct Action{bool enabled,interaction_enabled;};
struct Widget{
    std::shared_ptr<State> state;
    ii::core::Result<Snapshot> snapshot()const{if(!state||!state->alive)return ii::core::fail(ii::core::ErrorCode::closed,"retired");return Snapshot{state->enabled};}
    ii::core::Result<Interaction> interaction()const{if(!state||!state->alive)return ii::core::fail(ii::core::ErrorCode::closed,"retired");return Interaction{true,state->enabled,true,false};}
    ii::core::Result<Action> action_options()const{return Action{state->enabled,true};}
};
using Event=lvui::native::ButtonEventValue<Widget>;using Method=lvui::native::ButtonMethodValue<Event>;
using Registry=lvui::native::ButtonMethodRegistryValue<Event>;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){
    auto state=std::make_shared<State>();Widget widget{state};std::vector<int> calls;
    Registry methods;methods.set_method([&](const Event& event){require(event.enabled&&event.effective_enabled&&event.tone==lvui::ButtonTone::normal,"event values changed");calls.push_back(1);});
    auto gathered=lvui::native::collect_button_methods<Event>(std::vector<std::vector<Method>>{{[&](const Event&){calls.push_back(2);}}});
    require(bool(methods.set_methods(gathered)),"valid methods rejected");
    require(lvui::native::button_method_active(widget)&&bool(lvui::native::invoke_button_methods(widget,methods,true,lvui::ButtonTone::normal,"clicked")),"method dispatch failed");
    require(calls==std::vector<int>({1,2}),"method ordering changed");
    require(!methods.set_methods(std::vector<Method>(257,[](const Event&){})),"method limit not enforced");
    calls.clear();methods.set_method([&](const Event&){calls.push_back(3);state->alive=false;});
    require(bool(lvui::native::invoke_button_methods(widget,methods,true,lvui::ButtonTone::normal,"clicked"))&&calls==std::vector<int>{3},"dispatch continued after retirement");
}
