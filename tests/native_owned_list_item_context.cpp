#include <LVUI/NativeListItemComposite.h>
#include "list_item_composite_test_binding.h"
#include <iiGUI/WidgetState.h>

#include <any>
#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}return std::move(*value);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
struct Command final : lvui::ListFooterCommand { unsigned& count;explicit Command(unsigned& value):count(value){}void invoke()const override{++count;} };
}
int main(){
    using namespace ii::gui;using namespace lvui;using namespace lvui::native;
    auto tree=std::make_shared<ii::gui::detail::WidgetState>([](ii::runtime::Task task){task();});
    auto host=take(tree->content_host({},"host",{500,400}));std::optional<NativeListItemComposite> item;
    std::array<std::shared_ptr<const ListItemContext>,4> contexts;unsigned before_publish=0,command_calls=0,more_commands=0,actions=0;
    std::string last_action;
    std::optional<ListItemActionValues> action_payload;
    std::vector<std::pair<std::string,ListItemValue>> edits;std::vector<std::string> changes;
    ListItemCompositeInput input;input.row.configuration.variant=ListItemVariant::toggle;input.row.configuration.label="Alpha";
    input.checked=false;input.quantity=1;input.minimum_quantity=0;input.maximum_quantity=10;
    input.selector.options={{"A",true},{"B",true}};input.unit_selector.options={{"px",true},{"cm",true}};
    input.primary_action.command=std::make_shared<Command>(command_calls);
    input.more_menu.command=std::make_shared<Command>(more_commands);
    const auto view=[&](std::size_t index)->ListItemContextViewFactory{return [&,index](WidgetBuilder& builder,std::shared_ptr<const ListItemContext> context)->ii::core::Result<void>{
        contexts[index]=context;if(!context->edit_value("checked",true))++before_publish;
        auto child=builder.action_surface({},"view-"+std::to_string(index),{.text="View",.width=32,.height=20},[]{});
        if(!child)return std::unexpected(child.error());return {};
    };};
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto stack=builder.vstack({},"stack",{.spacing=0,.alignment=StackAlignment::leading});if(!stack)return std::unexpected(stack.error());
        auto value=NativeListItemComposite::create(builder,*stack,"item",input,{
            .checked_changed=[&](bool){changes.push_back("checked");},
            .edited=[&](std::string name,const ListItemValue& value){edits.emplace_back(std::move(name),value);},
            .action_triggered=[&](std::string name,const ListFooterSlot&){last_action=name;if(name=="primary")++actions;},
            .quantity_changed=[&](double){changes.push_back("quantity");},
            .selector_index_changed=[&](int){changes.push_back("selector");},
            .input_text_changed=[&](std::string){changes.push_back("input1");},
            .unit_index_changed=[&](int){changes.push_back("unit");},
            .segment_index_changed=[&](int){changes.push_back("segment");},
            .input_text2_changed=[&](std::string){changes.push_back("input2");},
            .action_event=[&](std::string,const ListItemActionPayload& value){if(auto* custom=std::get_if<ListItemActionValues>(&value);custom&&custom->custom_payload)action_payload=*custom;}},
            {.footer_context=view(0),.preview_context=view(1),.leading_context=view(2),.trailing_context=view(3)});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    require(before_publish==4&&contexts[0]&&contexts[0]==contexts[1]&&contexts[1]==contexts[2]&&contexts[2]==contexts[3],
            "Custom views did not receive one inert context during construction");
    auto context=contexts[0];require(context->get().label=="Alpha"&&!context->get().checked&&context->get().quantity==1,
            "Context did not expose the initial owner state");
    require(take(item->context())==context,"Public context accessor returned a different owner");
    unsigned observations=0;auto subscription=context->observe([&](const auto&,std::uint64_t){++observations;});
    auto next=take(item->options());next.row.configuration.label="Beta";good(item->set_options(next));
    require(context->get().label=="Beta"&&observations==1,"Owner option changes were not published to the context");
    next.row.configuration.enabled=false;good(item->set_options(next));
    require(!take(context->edit_value("checked",true))&&!context->get().checked,"Disabled owner accepted a contextual edit");
    next.row.configuration.enabled=true;good(item->set_options(next));
    require(take(context->edit_value("checked",true))&&take(item->checked())&&context->get().checked&&
            changes==std::vector<std::string>{"checked"}&&edits.size()==1&&edits.back().first=="checked",
            "Contextual edit did not update the overridden toggle and callbacks");
    require(!take(context->edit_value("checked",true))&&edits.size()==1,"An unchanged contextual edit emitted an event");
    require(take(context->edit_value("quantity",25.0))&&take(item->quantity())==10&&context->get().quantity==10,
            "Contextual quantity was not clamped to the owner range");
    require(!take(context->step_quantity(1))&&take(context->step_quantity(-1))&&take(item->quantity())==9,
            "Contextual step did not respect bounds");
    next=take(item->options());next.stepper_arrow=lvui::StepperArrow::up;good(item->set_options(next));
    require(!take(context->step_quantity(-1))&&take(item->quantity())==9,"Contextual step ignored the configured direction");
    next.stepper_arrow=lvui::StepperArrow::up_down;good(item->set_options(next));
    require(!take(context->step_quantity(0))&&take(context->step_quantity(-3))&&take(item->quantity())==6&&
            take(context->step_quantity(-100))&&take(item->quantity())==0&&
            take(context->step_quantity(2))&&take(item->quantity())==2,
            "Contextual step did not use the full direction or respect the range");
    require(take(context->edit_value("selectorIndex",std::int64_t(1)))&&take(context->edit_value("unitIndex",std::int64_t(1)))&&
            take(context->edit_value("segmentIndex",std::int64_t(1)))&&take(context->edit_value("inputText1",std::string("First")))&&
            take(context->edit_value("inputText2",std::string("Second"))),"Contextual edit omitted an owner value");
    require(context->get().values.selector_index==1&&context->get().values.unit_index==1&&context->get().values.segment_index==1&&
            context->get().values.input_text1=="First"&&context->get().values.input_text2=="Second",
            "Context did not publish the edited values");
    require(!context->edit_value("checked",std::string("invalid"))&&!context->edit_value("selectorIndex",std::int64_t(3))&&
            !context->edit_value("segmentIndex",1.5)&&
            !take(context->edit_value("unknown",true)),"Invalid contextual edits were accepted");
    require(take(context->trigger_action("primary"))&&command_calls==1&&actions==1,"Contextual action did not invoke the configured action");
    auto payload=std::make_shared<const std::any>(std::string("asset-42"));
    require(take(context->trigger_action("primary",payload))&&command_calls==2&&actions==2&&action_payload&&
            action_payload->custom_payload==payload&&action_payload->quantity==2&&action_payload->checked,
            "Contextual action did not preserve its custom payload and value snapshot");
    require(take(context->trigger_action("rename",payload))&&more_commands==1&&last_action=="rename"&&
            action_payload&&action_payload->custom_payload==payload,"A custom contextual action lost its menu policy or name");
    require(!context->trigger_action(std::string("bad\xff",4),payload)&&more_commands==1,
            "An invalid contextual action name reached the command");
    bool null_rejected=false;try{(void)context->get().triggerAction("primary",static_cast<const char*>(nullptr));}
    catch(const std::invalid_argument&){null_rejected=true;}
    require(null_rejected,"A null action string payload was accepted");
    next=take(item->options());next.primary_action.enabled=false;good(item->set_options(next));
    require(!take(context->trigger_action("primary"))&&command_calls==2&&actions==2,"Disabled contextual action was triggered");
    std::optional<ii::core::ErrorCode> thread_error;std::thread worker([&]{auto result=context->edit_value("checked",false);if(!result)thread_error=result.error().code;});worker.join();
    require(thread_error==ii::core::ErrorCode::wrong_executor,"Context allowed mutation from the wrong thread");
    auto old=item->root();std::shared_ptr<const ListItemContext> failed_context;
    auto failed=host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=NativeListItemComposite::create(builder,{},"bad",input,{}, {.footer_context=[&](WidgetBuilder&,auto candidate)->ii::core::Result<void>{
            failed_context=std::move(candidate);return ii::core::fail(ii::core::ErrorCode::failed,"View rejected");}});
        if(!value)return std::unexpected(value.error());return {};
    });
    require(!failed&&old.snapshot()&&failed_context&&!failed_context->edit_value("checked",true),
            "Failed contextual view replaced the active generation or remained writable");
    auto conflict=host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=NativeListItemComposite::create(builder,{},"conflict",input,{},
            {.footer=[](WidgetBuilder&)->ii::core::Result<void>{return {};},.footer_context=view(0)});
        if(!value)return std::unexpected(value.error());return {};
    });
    require(!conflict&&old.snapshot(),"Conflicting view factories replaced the active generation");
    auto stale=context->get().editValue("checked",false);auto stale_action=context->get().triggerAction("primary",payload);const auto old_edits=edits.size();
    auto retire=context->observe([&](const auto&,std::uint64_t){good(host.clear_content());});
    require(take(context->edit_value("checked",false))&&edits.size()==old_edits&&!item->root().snapshot(),
            "Context observer retirement delivered a stale edit");
    auto closed=context->edit_value("checked",true);require(!closed&&closed.error().code==ii::core::ErrorCode::closed,
            "Retired context remained writable");
    bool threw=false;try{stale();}catch(const std::exception&){threw=true;}require(threw,"A retired contextual command remained callable");
    threw=false;try{stale_action();}catch(const std::exception&){threw=true;}require(threw,"A retired action payload remained callable");
    input.row.configuration.label="Successor";good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=NativeListItemComposite::create(builder,{},"successor",input);if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    auto successor=take(item->context());require(successor!=context&&successor->get().label=="Successor"&&take(successor->edit_value("checked",true)),
            "A context could not be created lazily for a new owner");
    threw=false;try{stale();}catch(const std::exception&){threw=true;}
    require(threw&&take(item->checked()),"A retired command affected a successor item");
    {
        auto bad_observer=successor->observe([](const auto&,std::uint64_t){throw std::runtime_error("observer failed");});
        require(take(successor->edit_value("checked",false))&&!take(item->checked())&&successor->get().checked==false&&
                take(item->failure()).has_value(),"A failing observer lost the committed value or error");
    }
    next=take(item->options());next.row.configuration.label="Recovered";good(item->set_options(next));
    require(!take(item->failure()).has_value(),"A successful update did not clear a prior observer failure");
    good(host.set_content([&](WidgetBuilder& builder)->ii::core::Result<void>{
        auto value=NativeListItemComposite::create(builder,{},"throwing",input,
            {.edited=[](std::string,const ListItemValue&){throw std::runtime_error("edit callback failed");}});
        if(!value)return std::unexpected(value.error());item=*value;return {};
    }));
    auto throwing=take(item->context());auto edit_failure=throwing->edit_value("checked",true);
    require(!edit_failure&&edit_failure.error().code==ii::core::ErrorCode::failed&&take(item->checked())&&
            take(item->failure()).has_value(),"A failing edit callback lost the committed value or error");
    tree->close();
}
