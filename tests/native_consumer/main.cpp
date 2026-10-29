#include <LVUI/NativeValues.h>
#include <LVUI/NativeFunctionVariable.h>
#include <LVUI/NativeCollection.h>
#include <LVUI/NativeVirtualCollection.h>
#include <LVUI/NativeMeasuredCollection.h>
#include <LVUI/NativeCollectionNavigation.h>
#include <LVUI/NativeBackendFactories.h>
#include <LVUI/NativeComponentInputs.h>
#include <LVUI/NativeObjectInputs.h>
#include <LVUI/NativeMethods.h>
#include <LVUI/NativeInferredValues.h>
#include <LVUI/NativeLayoutValues.h>
#include <LVUI/NativeWindowValues.h>
#include <iostream>
struct Injected final {
    explicit Injected(int value):value(value){}
    Injected(const Injected&)=delete;Injected(Injected&&)=delete;
    int value;
};
int main() {
    auto native_color=lvui::native::color<iiColorManager::Rgba>("#ff000080");
    if(native_color!=iiColorManager::Rgba{1,0,0,128/255.})return 100;
    ii::core::Observable<int> source{2};
    lvui::native::Bindings bindings;
    int shown = 0;
    auto binding = bindings.bind([&](lvui::native::ReadContext& context) {
        return lvui::native::integer(lvui::native::multiply(context.read(source), 3));
    }, [&](std::int64_t value) -> ii::core::Result<void> { shown = int(value); return {}; });
    if (!binding || shown != 6) return 1;
    source.set(4);
    if (shown != 12 || !bindings.close()) return 2;
    source.set(5);
    if (shown != 12) return 3;
    lvui::native::KeyedRows<int> rows;
    auto key=[](int value,std::size_t){return value;};
    auto mount=[](const auto&)->ii::core::Result<void>{return {};};
    if(!rows.apply(lvui::native::collection_frame(std::vector{1,2},key),mount))return 4;
    auto first=rows.find("1");
    if(!rows.apply(lvui::native::collection_frame(std::vector{2,1},key),mount)||rows.find("1")!=first||first->state.get().index!=1)return 5;
    if(!rows.close())return 6;
    auto virtual_frame=lvui::native::virtual_collection_frame(std::vector{1,2,3,4,5},key,{.item_height=40,.overscan=0},{.width=100,.height=40,.y=80});
    if(virtual_frame.collection.items.size()!=1||virtual_frame.collection.items[0].value!=3||virtual_frame.content_height!=200)return 7;
    auto indexed=lvui::native::virtual_collection_source(std::vector{1,2,3},key);
    lvui::native::VirtualMeasurements<int> measurements;
    auto measured=measurements.plan(indexed,{.item_height=40,.measure_height=true},{.width=100,.height=70});
    measurements.report(measured.measurement_generation,{{"1",100,10},{"2",100,60}});
    auto actual=measurements.plan(indexed,{.item_height=40,.measure_height=true},{.width=100,.height=70});
    if(actual.cells[1].y!=10||actual.content_height!=110)return 8;
    measurements.close();
    lvui::native::CollectionNavigation navigation;
    navigation.configure({.mode=lvui::native::CollectionSelectionMode::multiple});
    auto moved=navigation.command(*indexed,lvui::native::CollectionCommand::end);
    if(!moved.handled||navigation.state.get().current_key!="3")return 9;
    navigation.command(*indexed,lvui::native::CollectionCommand::previous,{}, {.range=true});
    if(navigation.state.get().selected_keys!=std::vector<std::string>{"2","3"})return 10;
    navigation.close();
    auto owner=std::make_shared<int>(0);auto callbacks=std::make_shared<lvui::native::Bindings>();
    ii::core::Observable<int> amount{2};
    auto action=lvui::native::returned_callback(owner,callbacks,[&](lvui::native::ReadContext& context){
        return [amount=context.read(amount)]{return amount;};
    },[](const auto& owner,auto& handler){*owner+=handler();});
    action();amount.set(3);action();if(*owner!=5)return 11;
    if(!callbacks->close())return 12;action();if(*owner!=5)return 13;
    auto factories=lvui::native::BackendFactories{}.with<Injected>([value=std::make_unique<int>(7)](const lvui::native::BackendRequest& request){return Injected(*value+(request.binding=="second"?1:0));});
    auto injected=factories.create<Injected>("Injected","second");if(injected.value!=8)return 14;
    int typed=0;
    auto handler=[&](std::string text,bool value){typed=int(text.size())+(value?1:0);};
    lvui::native::invoke_callback<void(std::string,bool)>(handler,std::string_view("Native"),true);
    if(typed!=7)return 15;
    auto shared_owner=std::make_shared<int>(0);auto shared_graph=std::make_shared<lvui::native::Bindings>();
    ii::core::Observable<int> generation{1};int made=0;
    auto shared_value=lvui::native::make_callback_value<void(std::int64_t)>(shared_owner,shared_graph,
        [&](lvui::native::ReadContext& context){++made;return [owner=shared_owner,state=std::make_unique<int>(context.read(generation))](std::int64_t n){*owner+=++*state+int(n);};},
        [](const auto&,auto& handler,std::int64_t n){handler(n);});
    auto second_value=shared_value;(*shared_value)(0);(*second_value)(0);
    if(*shared_owner!=5||made!=1)return 16;
    generation.set(3);(*second_value)(0);if(*shared_owner!=9||made!=2)return 17;
    if(!shared_graph->close())return 18;std::weak_ptr<int> retired_owner=shared_owner;shared_owner.reset();
    (*second_value)(99);if(!retired_owner.expired())return 19;
    auto function_owner=std::make_shared<int>(0);auto function_graph=std::make_shared<lvui::native::Bindings>();
    auto function=lvui::native::make_callback_value<std::string(std::int64_t)>(function_owner,function_graph,
        [&](lvui::native::ReadContext& context){return [n=context.read(generation)](std::int64_t input){return std::to_string(n+input);};},
        [](const auto&,auto& fn,std::int64_t input){return fn(input);});
    auto display=std::make_shared<lvui::native::Bindings>();std::string shown_function;
    auto connected=display->bind([function](lvui::native::ReadContext& context){return function->read(context,2);},[&](const std::string& text)->ii::core::Result<void>{shown_function=text;return {};});
    if(!connected||shown_function!="5")return 20;generation.set(6);if(shown_function!="8")return 21;
    auto frozen_function=lvui::native::snapshot_callback_value(function_owner,function_graph,function);
    generation.set(9);if(shown_function!="11"||(*frozen_function)(2)!="8")return 22;
    if(!display->close()||!function_graph->close())return 23;
    bool closed_function=false;try{(void)(*function)(0);}catch(const std::logic_error&){closed_function=true;}if(!closed_function)return 24;
    bool closed_snapshot=false;try{(void)(*frozen_function)(0);}catch(const std::logic_error&){closed_snapshot=true;}if(!closed_snapshot)return 25;
    auto variable_owner=std::make_shared<int>(0);auto variable_graph=std::make_shared<lvui::native::Bindings>();
    using FunctionVariable=lvui::native::FunctionVariable<std::string(std::int64_t)>;
    auto variable=FunctionVariable::from_native(variable_owner,variable_graph,[]{return [](std::int64_t n){return std::to_string(n+1);};},
        [](const auto&,auto& fn,std::int64_t n){return fn(n);});
    auto selected=lvui::native::snapshot_callback_value(variable_owner,variable_graph,variable->value());
    if(!variable->replace_native([]{return [state=std::make_unique<int>(7)](std::int64_t n){return std::to_string(*state+n);};})||
       (*variable->value())(2)!="9"||(*selected)(2)!="3"||variable_graph->stats()->bindings!=2)return 26;
    if(!variable_graph->close()||variable->set(function))return 27;
    struct InputOwner { std::shared_ptr<lvui::native::ReadOnlyValue<int>> input; };
    struct InputChild { std::shared_ptr<InputOwner> parent; };
    lvui::native::Bindings input_graph;
    auto input_owner = std::make_shared<InputOwner>();
    auto input_child = std::make_shared<InputChild>(); input_child->parent = input_owner;
    int evaluations = 0;
    auto input = input_graph.derive([&](lvui::native::ReadContext& context) { ++evaluations; return context.read(source); });
    if (!input) return 28;
    input_owner->input = *input;
    auto select = [](auto& scope) -> decltype((scope.input)) { return (scope.input); };
    auto& first_input = lvui::native::component_input(input_child, select);
    auto& second_input = lvui::native::component_input(input_child.get(), select);
    if (&first_input != &second_input || first_input.get() != 5 || evaluations != 1) return 29;
    source.set(6);
    if (first_input.get() != 6 || second_input.get() != 6 || evaluations != 2) return 30;
    input_child->parent.reset();
    bool missing = false; try { (void)lvui::native::component_input(input_child, select); } catch (const std::logic_error&) { missing = true; }
    if (!missing) return 31;
    input_child->parent = input_owner; input_owner->input.reset();
    bool uninitialized = false; try { (void)lvui::native::component_input(input_child, select); } catch (const std::logic_error&) { uninitialized = true; }
    if (!uninitialized || !input_graph.close()) return 32;
    const auto layout=lvui::native::stack_layout_value({.spacing=-1,.default_spacing=6,.padding=4,.left_padding=2,
        .alignment={lvui::native::LayoutAlignment::trailing,lvui::native::LayoutAlignment::leading},.width=200,.height=80});
    if(layout.spacing!=6 || layout.insets!=lvui::native::LayoutInsets{2,4,4,4} || layout.width!=200 || layout.height!=80) return 33;
    bool invalid_layout=false;
    try{(void)lvui::native::stack_layout_value({.width=-1});}catch(const std::invalid_argument&){invalid_layout=true;}
    if(!invalid_layout)return 34;
    enum class Accent {blue,green=6};
    if(lvui::native::window_accent<Accent>("green")!=Accent::green)return 35;
    struct Record final {std::string title;int count;};
    ii::core::Observable<int> record_source{1};const auto& readonly_source=record_source;
    lvui::native::Bindings record_graph;
    auto record=record_graph.derive([&](lvui::native::ReadContext& context){return Record{"Native",context.read(readonly_source)};});
    if(!record||(*record)->get().count!=1)return 36;
    record_source.set(2);if((*record)->get().count!=2||!record_graph.close())return 37;
    auto native_input=std::make_shared<const Record>(Record{"Input",8});
    auto stored_input=lvui::native::object_input<Record>(native_input);
    if(stored_input!=native_input||stored_input->count!=8)return 38;
    bool null_input=false;
    try{(void)lvui::native::object_input<Record>(std::shared_ptr<const Record>{});}
    catch(const std::invalid_argument&){null_input=true;}
    if(!null_input||lvui::native::object_input<Record>(Record{"Value",5}).count!=5)return 39;
    struct Query final {ii::core::Observable<int> amount{3};int read()const{return amount.get()*2;}} query;
    lvui::native::Bindings queries;int query_result=0;
    auto query_binding=queries.bind([&](auto& context){return lvui::native::Methods::query(context,query,[](const auto& object){return object.read();});},[&](int value)->ii::core::Result<void>{query_result=value;return {};});
    if(!query_binding||query_result!=6)return 40;query.amount.set(7);if(query_result!=14||!queries.close())return 41;
    struct DeferredItem final {int calls=0;void act(){++calls;}};
    struct DeferredOwner final {ii::core::Observable<std::shared_ptr<DeferredItem>> selected{std::make_shared<DeferredItem>()};};
    auto deferred_owner=std::make_shared<DeferredOwner>();auto deferred_graph=std::make_shared<lvui::native::Bindings>();
    auto deferred=lvui::native::make_callback_value<void()>(deferred_owner,deferred_graph,[deferred_owner](lvui::native::ReadContext& context){
        return lvui::native::Methods::reference(context,[deferred_owner](lvui::native::ReadContext& context)->decltype(auto){return context.read(deferred_owner->selected);},
            [](auto&& object)->decltype(object.act()){return object.act();});
    },[](const auto&,auto& handler){handler();});
    auto deferred_first=deferred_owner->selected.get();if(deferred_first->calls!=0)return 42;(*deferred)();
    auto deferred_second=std::make_shared<DeferredItem>();deferred_owner->selected.set(deferred_second);(*deferred)();
    if(deferred_first->calls!=1||deferred_second->calls!=1||!deferred_graph->close())return 43;
    (*deferred)();if(deferred_second->calls!=1)return 44;
    struct FieldOwner final {ii::core::Observable<std::function<int()>> run{[n=0]()mutable{return ++n;}};};
    auto field_owner=std::make_shared<FieldOwner>();auto field_graph=std::make_shared<lvui::native::Bindings>();
    auto field=lvui::native::make_callback_value<std::int64_t()>(field_owner,field_graph,[field_owner](lvui::native::ReadContext& context){
        return lvui::native::Methods::reference(context,[field_owner](lvui::native::ReadContext&)->FieldOwner&{return *field_owner;},
            [](auto&& object)->decltype(object.run()){return object.run();},
            [](auto&& object)->decltype((object.run)){return (object.run);});
    },[](const auto&,auto& selected){return selected();});
    auto field_frozen=lvui::native::snapshot_callback_value(field_owner,field_graph,field);
    if((*field)()!=1)return 45;field_owner->run.set([]{return 9;});
    if((*field)()!=9||(*field_frozen)()!=2)return 46;
    field_owner->run.set({});if((*field)()!=9||field_graph->failures()->empty())return 47;
    if(!field_graph->close())return 48;
    bool field_retired=false;try{(void)(*field)();}catch(const std::logic_error&){field_retired=true;}
    if(!field_retired)return 49;
    struct FactoryNode final {int count=0;std::function<void()> function=[this]{++count;};auto& make(){return function;}};
    struct FactoryOwner final {ii::core::Observable<std::shared_ptr<FactoryNode>> selected{std::make_shared<FactoryNode>()};};
    auto factory_owner=std::make_shared<FactoryOwner>();auto factory_graph=std::make_shared<lvui::native::Bindings>();
    auto captured=lvui::native::make_callback_value<void()>(factory_owner,factory_graph,[factory_owner](lvui::native::ReadContext& context){
        return lvui::native::Methods::callable(context,[factory_owner](lvui::native::ReadContext& context)->decltype(auto){
            return lvui::native::Methods::receiver_member(context,[factory_owner](lvui::native::ReadContext& context)->decltype(auto){return context.read(factory_owner->selected);},[](auto& node)->decltype(auto){return node.make();});
        });
    },[](const auto&,auto& selected){selected();});
    std::weak_ptr<FactoryNode> old_factory_node=factory_owner->selected.get();(*captured)();
    auto next_factory_node=std::make_shared<FactoryNode>();factory_owner->selected.set(next_factory_node);(*captured)();
    if(!old_factory_node.expired()||next_factory_node->count!=1)return 50;
    auto captured_argument=lvui::native::make_callback_value<std::string()>(factory_owner,factory_graph,[](lvui::native::ReadContext& context){
        return lvui::native::Methods::callable(context,[](lvui::native::ReadContext& context){
            auto&& text=lvui::native::Methods::argument(context,[](lvui::native::ReadContext&){return std::string("Retained");});
            return [&text]{return text;};
        });
    },[](const auto&,auto& selected){return selected();});
    if((*captured_argument)()!="Retained"||!factory_graph->close())return 51;
    (*captured)();if(next_factory_node->count!=1)return 52;
    struct InferredOwner final {ii::core::Observable<int> count{2};auto make(){return [n=count.get()](std::int64_t value){return std::to_string(n+value);};}};
    auto inferred_owner=std::make_shared<InferredOwner>();auto inferred_graph=std::make_shared<lvui::native::Bindings>();
    auto inferred_initial=[inferred_owner]{return lvui::native::Methods::inferred_result([inferred_owner](lvui::native::ReadContext&){return inferred_owner->make();},lvui::native::inferred::Marker{});};
    auto inferred_formula=[inferred_owner](lvui::native::ReadContext& context){return lvui::native::Methods::inferred_result(context,[inferred_owner](lvui::native::ReadContext& context){(void)context.read(inferred_owner->count);return inferred_owner->make();},lvui::native::inferred::Marker{});};
    auto inferred=lvui::native::inferred::make<lvui::native::inferred::Mode::readonly>(inferred_owner,inferred_graph,inferred_initial,inferred_formula);
    std::uint64_t inferred_changes=0;auto inferred_watch=inferred->observe([&](const auto&,std::uint64_t){++inferred_changes;});
    if(lvui::native::inferred::call(*inferred,3.0)!="5")return 53;
    inferred_owner->count.set(8);
    if(lvui::native::inferred::call(*inferred,3.0)!="11"||inferred_changes!=1||!inferred_graph->close())return 54;
    std::cout << "Relocated LVUI::Native consumer passed\n";
}
