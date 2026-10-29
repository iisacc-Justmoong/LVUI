#include <LVUI/NativeFunctionVariable.h>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace lvui::native;
namespace {
using Signature=std::string(std::int64_t);
using Variable=FunctionVariable<Signature>;
using Value=CallbackValue<Signature>;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void good(ii::core::Result<void> value){if(!value){if(value.error().cause)std::rethrow_exception(value.error().cause);throw std::runtime_error(value.error().message);}}
struct Owner final {ii::core::Observable<int> amount{1};std::function<void()> during;};
auto dynamic(const std::shared_ptr<Owner>& owner,const std::shared_ptr<Bindings>& graph){
    return make_callback_value<Signature>(owner,graph,[owner](ReadContext& context){
        return [state=std::make_unique<int>(context.read(owner->amount))](std::int64_t input){return std::to_string(*state+input);};
    },[](const auto&,auto& handler,std::int64_t input){return handler(input);});
}
auto constant(int amount){return std::make_shared<Value>([amount](std::int64_t input){return std::to_string(amount+input);});}
void versions(){
    auto owner=std::make_shared<Owner>();auto origin=std::make_shared<Bindings>();auto graph=std::make_shared<Bindings>();
    auto source=dynamic(owner,origin);auto variable=Variable::from_value(owner,graph,source);auto value=variable->value();
    auto display=std::make_shared<Bindings>();std::string shown;int evaluations=0;
    auto bound=display->bind([value,&evaluations](ReadContext& context){++evaluations;return value->read(context,2);},
        [&](const std::string& text)->ii::core::Result<void>{shown=text;return {};});
    check(bool(bound)&&shown=="3","A mutable function did not expose its initial result");
    auto frozen=snapshot_callback_value(owner,graph,value);owner->amount.set(5);
    check(shown=="3","An initializer replaced a mutable function without assignment");
    good(variable->set(source));check(shown=="7"&&(*frozen)(2)=="3"&&value==variable->value(),"Assignment lost function identity, version selection or the Static snapshot");
    const auto before=evaluations;good(variable->set(value));check(evaluations==before,"Self-assignment created another function wrapper or published a new version");
    auto invalid=variable->set({});check(!invalid&&invalid.error().code==ii::core::ErrorCode::invalid_argument&&shown=="7","Null assignment changed the valid function");
    bool ready=true;
    auto gated=std::make_shared<Value>([](std::int64_t n){return std::to_string(n);},std::function<void(ReadContext&)>{},[&]{return ready;});
    good(variable->set(gated));ready=false;bool closed=false;try{(void)(*value)(2);}catch(const std::logic_error&){closed=true;}
    check(closed,"Selecting an opaque function lost its explicit readiness gate");ready=true;
    ii::core::Observable<int> scalar{1};good(assign_value(scalar,1));good(assign_value(scalar,2));check(scalar.get()==2,"Scalar assignment no longer accepted Observable change flags");
    good(assign_value(*variable,constant(9)));check(shown=="11","The generated setter adapter ignored a function assignment");
    good(display->close());good(graph->close());good(origin->close());
}
void native(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();int factories=0;std::weak_ptr<int> retired;
    auto variable=Variable::from_native(owner,graph,[&]{++factories;auto resource=std::make_shared<int>(0);retired=resource;return [resource,state=std::make_unique<int>(1)](std::int64_t input){return std::to_string(++*state+input);};},
        [](const auto&,auto& handler,std::int64_t input){return handler(input);});
    check((*variable->value())(0)=="2"&&factories==1,"A native initializer was copied or evaluated repeatedly");
    good(variable->replace_native([]{return [state=std::make_unique<int>(7)](std::int64_t input){return std::to_string(*state+input);};}));
    check((*variable->value())(2)=="9"&&retired.expired(),"Replacing a native initializer retained its old resources");
    struct Immovable final {Immovable()=default;Immovable(const Immovable&)=delete;Immovable(Immovable&&)=delete;std::string operator()(std::int64_t n)const{return std::to_string(n);}};
    good(variable->replace_native([]{return Immovable{};}));check((*variable->value())(4)=="4","A native replacement unnecessarily moved its callable result");
    using Pointer=std::string(*)(std::int64_t);Pointer pointer=+[](std::int64_t n){return std::to_string(n);};
    auto borrowed=Variable::from_native(owner,graph,[&]()->Pointer&{return pointer;},
        [](const auto&,auto& handler,std::int64_t n){return std::invoke(handler,n);});
    check((*borrowed->value())(2)=="2","A native function pointer was not callable");pointer=nullptr;
    bool empty=false;try{(void)(*borrowed->value())(2);}catch(const std::invalid_argument&){empty=true;}
    check(empty,"A borrowed pointer becoming empty was invoked without validation");
    int calls=0;using Event=FunctionVariable<void(std::int64_t)>;
    auto event=Event::from_native(owner,graph,[]{return [](std::int64_t){};},[](const auto&,auto& fn,std::int64_t n){fn(n);});
    good(event->replace_native([&]{return [&calls](std::int64_t n){calls+=int(n);};}));(*event->value())(3);check(calls==3,"A Void function variable did not invoke its replacement");
    good(graph->close());(*event->value())(3);check(calls==3,"A closed Void function variable admitted a new event");
}
void rollback(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto variable=Variable::from_value(owner,graph,constant(1));
    auto bad=std::make_shared<Value>([](std::int64_t){return std::string("bad");},std::function<void(ReadContext&)>{},std::function<bool()>{},
        []()->std::shared_ptr<Value::Action>{throw std::runtime_error("Snapshot failure");});
    auto result=variable->set(bad);check(!result&&result.error().cause&&(*variable->value())(2)=="3","A failing replacement discarded the previous function");
    auto readiness=std::make_shared<Value>([](std::int64_t){return std::string{};},std::function<void(ReadContext&)>{},[]()->bool{throw std::runtime_error("Readiness failure");});
    result=variable->set(readiness);check(!result&&result.error().cause&&(*variable->value())(2)=="3","A readiness error escaped the replacement result contract");
    auto empty=std::make_shared<Value>([](std::int64_t){return std::string{};},std::function<void(ReadContext&)>{},std::function<bool()>{},[](){return std::shared_ptr<Value::Action>{};});
    result=variable->set(empty);check(!result&&(*variable->value())(2)=="3","An empty selected function version was accepted");
    using Pointer=std::string(*)(std::int64_t);
    result=variable->replace_native([]{return Pointer{};});
    check(!result&&(*variable->value())(2)=="3","A null native function pointer was accepted as a replacement");
    result=variable->replace_native([]{return Value::Action{};});
    check(!result&&(*variable->value())(2)=="3","An empty native function wrapper was accepted as a replacement");
    result=variable->replace_native([]()->Value::Action{throw std::runtime_error("Factory failure");});
    check(!result&&(*variable->value())(2)=="3","A failed native factory changed the valid function");
    auto closed_graph=std::make_shared<Bindings>();auto closed=dynamic(owner,closed_graph);good(closed_graph->close());
    result=variable->set(closed);check(!result&&result.error().code==ii::core::ErrorCode::closed&&(*variable->value())(2)=="3","A closed source replaced a valid function");
    good(variable->set(constant(5)));check((*variable->value())(2)=="7","A function variable did not recover after rejected assignments");
    std::shared_ptr<Value> disappearing;
    disappearing=std::make_shared<Value>([](std::int64_t){return std::string{};},std::function<void(ReadContext&)>{},std::function<bool()>{},
        [&]{disappearing.reset();return std::make_shared<Value::Action>([](std::int64_t n){return std::to_string(n+5);});});
    good(variable->set(disappearing));check(!disappearing&&(*variable->value())(2)=="7","An input removing itself during selection lost its function metadata");
    std::weak_ptr<Variable> retired=variable;
    result=variable->replace_native([&]{good(graph->close());variable.reset();return [](std::int64_t){return std::string("late");};});
    check(!result&&result.error().code==ii::core::ErrorCode::closed&&retired.expired(),"A factory retiring its own target mutated or retained the closed variable");
}
void lifetime(){
    struct Resident final {Resident()=default;Resident(const Resident&)=delete;Resident(Resident&&)=delete;std::string operator()(std::int64_t n)const{return std::to_string(n);}};
    struct Scope final {Resident resident;};
    auto owner=std::make_shared<Scope>();auto graph=std::make_shared<Bindings>();
    auto variable=Variable::from_native(owner,graph,[owner]()->const Resident&{return owner->resident;},
        [](const auto&,auto& handler,std::int64_t input){static_assert(std::is_const_v<std::remove_reference_t<decltype(handler)>>);return handler(input);});
    auto value=variable->value();check((*value)(2)=="2","A borrowed const native initializer was changed");
    std::weak_ptr<Scope> retired=owner;owner.reset();check(!retired.expired(),"A variable graph did not own its active scope");
    good(graph->close());check(retired.expired(),"A saved variable retained its retired scope");
    bool rejected=false;try{(void)(*value)(0);}catch(const std::logic_error&){rejected=true;}
    auto replacement=variable->set(constant(3));check(rejected&&!replacement&&replacement.error().code==ii::core::ErrorCode::closed,"A retired variable admitted a read or write");
    auto origin_owner=std::make_shared<Owner>();auto origin_graph=std::make_shared<Bindings>();auto source=dynamic(origin_owner,origin_graph);
    auto first_owner=std::make_shared<int>(0);auto first_graph=std::make_shared<Bindings>();auto first=Variable::from_value(first_owner,first_graph,source);
    auto second_owner=std::make_shared<int>(0);auto second_graph=std::make_shared<Bindings>();auto second=Variable::from_value(second_owner,second_graph,first->value());
    good(first_graph->close());check((*second->value())(2)=="3","A copied native version depended on its old variable storage");
    good(origin_graph->close());rejected=false;try{(void)(*second->value())(2);}catch(const std::logic_error&){rejected=true;}
    check(rejected,"A copied native version escaped its actual receiver lifetime");good(second_graph->close());
}
void active(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();std::weak_ptr<int> resource;
    auto variable=Variable::from_native(owner,graph,[&]{auto token=std::make_shared<int>(0);resource=token;return [receiver=owner.get(),token](std::int64_t n){if(auto fn=std::exchange(receiver->during,{}))fn();return std::to_string(n);};},
        [](const auto&,auto& handler,std::int64_t n){return handler(n);});
    auto value=variable->value();owner->during=[&]{good(variable->set(constant(10)));check(!resource.expired(),"Replacement destroyed an admitted native function");};
    check((*value)(2)=="2"&&(*value)(2)=="12"&&resource.expired(),"Replacement confused active and subsequent function versions");
    good(variable->replace_native([&]{return [receiver=owner.get()](std::int64_t n){if(auto fn=std::exchange(receiver->during,{}))fn();return std::to_string(n);};}));
    std::weak_ptr<Owner> retired=owner;
    owner->during=[&]{good(graph->close());owner.reset();variable.reset();check(!retired.expired(),"Self-removal destroyed an admitted receiver");};
    check((*value)(3)=="3"&&retired.expired(),"Self-removing replacement lost its result or kept the receiver");
}
void threading(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto variable=Variable::from_value(owner,graph,constant(1));
    bool write_rejected=false,creation_rejected=false;int factory_calls=0,ready_calls=0;std::shared_ptr<Value> foreign;
    std::thread worker([&]{
        auto result=variable->set(constant(9));write_rejected=!result&&result.error().code==ii::core::ErrorCode::wrong_executor;
        try{(void)Variable::from_native(owner,graph,[&]{++factory_calls;return [](std::int64_t){return std::string{};};},[](const auto&,auto& fn,std::int64_t n){return fn(n);});}catch(const std::logic_error&){creation_rejected=true;}
        foreign=std::make_shared<Value>([](std::int64_t){return std::string{};},std::function<void(ReadContext&)>{},[&]{++ready_calls;return true;});
    });worker.join();
    auto result=variable->set(foreign);
    check(write_rejected&&creation_rejected&&factory_calls==0&&!result&&result.error().code==ii::core::ErrorCode::wrong_executor&&ready_calls==0,
          "A function write, native factory or foreign readiness check escaped its execution domain");
    good(graph->close());
}
void bounds(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto variable=Variable::from_value(owner,graph,constant(1));
    const auto bindings=graph->stats()->bindings;std::weak_ptr<int> previous;
    for(int i=0;i<1000;++i){auto token=std::make_shared<int>(i);auto next=std::weak_ptr(token);
        good(variable->replace_native([token]{return [token](std::int64_t n){return std::to_string(*token+n);};}));
        check(previous.expired()&&(*variable->value())(1)==std::to_string(i+1),"Replacement retained obsolete captures or invoked a stale function");previous=next;
    }
    check(graph->stats()->bindings==bindings&&bindings==1,"Replacement added unbounded graph nodes");
    auto other_owner=std::make_shared<Owner>();auto other_graph=std::make_shared<Bindings>();auto other=Variable::from_value(other_owner,other_graph,variable->value());
    const auto before=graph->stats()->evaluations;
    for(int i=0;i<1000;++i){good(variable->set(other->value()));good(other->set(variable->value()));}
    check(graph->stats()->evaluations==before&&other_graph->stats()->bindings==1,"Copying selected versions built forwarding chains or published redundant updates");
    good(other_graph->close());good(graph->close());check(previous.expired(),"Variable closure retained its final capture");
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"versions";
    if(name=="versions")versions();else if(name=="native")native();else if(name=="rollback")rollback();else if(name=="lifetime")lifetime();else if(name=="active")active();else if(name=="threading")threading();else if(name=="bounds")bounds();else throw std::runtime_error("Unknown function variable test");
    std::cout<<"Native function variable "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
