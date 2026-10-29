#include <LVUI/NativeValues.h>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace lvui::native;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Owner final {ii::core::Observable<int> amount{1};int made=0;std::function<void()> during;};
auto make(const std::shared_ptr<Owner>& owner,const std::shared_ptr<Bindings>& graph){
    return make_callback_value<std::string(std::int64_t)>(owner,graph,[owner](ReadContext& context){
        ++owner->made;const auto amount=context.read(owner->amount);if(amount<0)throw std::runtime_error("Function factory failed");
        return [state=std::make_unique<int>(amount),receiver=owner.get()](std::int64_t input){
            if(auto during=std::exchange(receiver->during,{}))during();return std::to_string(*state+input);
        };
    },[](const auto&,auto& handler,std::int64_t input){return handler(input);});
}
void results(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto value=make(owner,graph);
    check((*value)(2)=="3","A native function lost its result");
    check(invoke_callback<bool(int)>([](int n){return n>0;},2),"A Bool result was not returned");
    check(invoke_callback<std::int64_t()>([]{return 7;})==7,"An integral native result did not satisfy Int");
    struct Immovable final {int value=9;Immovable()=default;Immovable(const Immovable&)=delete;Immovable(Immovable&&)=delete;};
    const auto object=invoke_callback<Immovable()>([]{return Immovable{};});check(object.value==9,"A native result was copied or moved unnecessarily");
    std::shared_ptr<CallbackValue<std::string()>> disappearing;
    disappearing=std::make_shared<CallbackValue<std::string()>>([]{return std::string("safe");},std::function<void(ReadContext&)>{},[&]{disappearing.reset();return true;});
    check((*disappearing)()=="safe"&&!disappearing,"Self-removal in a readiness callback destroyed active function metadata");
    auto consumer=std::make_shared<Bindings>();std::shared_ptr<CallbackValue<std::string()>> tracked;
    tracked=std::make_shared<CallbackValue<std::string()>>([]{return std::string("tracked");},[&](ReadContext&){tracked.reset();});
    std::string shown;auto connection=consumer->bind([&](ReadContext& context){return tracked->read(context);},[&](const std::string& text)->ii::core::Result<void>{shown=text;return {};});
    check(bool(connection)&&shown=="tracked"&&!tracked,"Self-removal during tracking destroyed active function metadata");check(bool(consumer->close()),"Metadata test close failed");
    check(bool(graph->close()),"Closing failed");
}
void reactive(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto value=make(owner,graph);
    auto consumer=std::make_shared<Bindings>();std::string shown;
    auto bound=consumer->bind([value](ReadContext& context){return value->read(context,2);},[&](const std::string& text)->ii::core::Result<void>{shown=text;return {};});
    check(bool(bound)&&shown=="3"&&owner->made==1,"A function result was not bound");
    owner->amount.set(4);check(shown=="6"&&owner->made==2,"Changing only the function value did not update its consumers");
    owner->amount.set(-1);check(shown=="6"&&!graph->failures()->empty(),"A failed function factory discarded its valid result");
    owner->amount.set(8);check(shown=="10"&&graph->failures()->empty(),"A function value did not recover");
    check(bool(consumer->close())&&bool(graph->close()),"Function graph close failed");
}
void lifetime(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto value=make(owner,graph);
    check(bool(graph->close()),"Function close failed");std::weak_ptr<Owner> retired=owner;owner.reset();
    bool rejected=false;try{(void)(*value)(0);}catch(const std::logic_error&){rejected=true;}
    check(rejected&&retired.expired(),"A closed function returned a fabricated value or retained its owner");
}
void threading(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto value=make(owner,graph);bool rejected=false;
    std::thread worker([&]{try{(void)(*value)(0);}catch(const std::logic_error&){rejected=true;}});worker.join();
    check(rejected&&owner->made==1,"A return-valued function accepted a foreign execution domain");
    check(bool(graph->close()),"Function close failed");
}
void snapshot(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();std::weak_ptr<int> resource;
    auto value=make_callback_value<std::string(std::int64_t)>(owner,graph,[owner,&resource](ReadContext& context){
        auto token=std::make_shared<int>(0);resource=token;const auto amount=context.read(owner->amount);
        return [state=std::make_unique<int>(amount),token=std::move(token)](std::int64_t input){return std::to_string(++*state+input);};
    },[](const auto&,auto& handler,std::int64_t input){return handler(input);});
    auto frozen_owner=std::make_shared<int>(0);auto frozen_graph=std::make_shared<Bindings>();
    auto frozen=snapshot_callback_value(frozen_owner,frozen_graph,value);auto old=resource;
    check((*value)(0)=="2"&&(*frozen)(0)=="3","A function snapshot copied its mutable handler instead of selecting the current version");
    owner->amount.set(5);
    check((*value)(0)=="6"&&(*frozen)(0)=="4"&&!old.expired(),"A snapshot followed a later function version or discarded its selected capture");
    check(frozen_graph->stats()->sources==0,"A Static function snapshot retained reactive source dependencies");
    auto repeated=snapshot_callback_value(frozen_owner,frozen_graph,frozen);
    check((*repeated)(0)=="5"&&(*frozen)(0)=="6","Snapshot forwarding lost selected version identity");
    int events=0;
    auto event=make_callback_value<void()>(owner,graph,[&events](ReadContext&){return [&events]{++events;};},
        [](const auto&,auto& handler){handler();});
    auto frozen_event=snapshot_callback_value(frozen_owner,frozen_graph,event);(*frozen_event)();
    check(events==1,"A Void function snapshot was not callable");
    check(bool(frozen_graph->close())&&old.expired(),"Closing a snapshot retained its selected native capture");
    (*frozen_event)();check(events==1,"A Void snapshot admitted an event after closure");
    bool rejected=false;try{(void)(*frozen)(0);}catch(const std::logic_error&){rejected=true;}
    check(rejected&&bool(graph->close()),"A closed snapshot produced a fabricated result");
}
void snapshot_lifetime(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto value=make(owner,graph);
    auto frozen_owner=std::make_shared<int>(0);auto frozen_graph=std::make_shared<Bindings>();
    auto frozen=snapshot_callback_value(frozen_owner,frozen_graph,value);bool foreign=false;
    std::thread worker([&]{try{(void)snapshot_callback_value(frozen_owner,frozen_graph,value);}catch(const std::logic_error&){foreign=true;}});worker.join();
    check(foreign,"Snapshot creation accepted a foreign execution domain");
    std::weak_ptr<int> retired=frozen_owner;
    owner->during=[&]{check(bool(frozen_graph->close()),"Snapshot self-close failed");frozen_owner.reset();check(!retired.expired(),"Closing a snapshot destroyed its admitted receiver before return");};
    check((*frozen)(2)=="3"&&retired.expired(),"A self-closing snapshot lost its result or retained its target owner");
    frozen_owner=std::make_shared<int>(0);frozen_graph=std::make_shared<Bindings>();
    auto later=snapshot_callback_value(frozen_owner,frozen_graph,value);check(bool(graph->close()),"Source close failed");
    std::weak_ptr<Owner> original=owner;owner.reset();bool rejected=false;
    try{(void)(*later)(0);}catch(const std::logic_error&){rejected=true;}
    check(rejected&&original.expired(),"A snapshot revived its retired source receiver");
    check(bool(frozen_graph->close()),"Snapshot cleanup failed");
    struct Resident final {
        mutable int calls=0;
        Resident()=default;Resident(const Resident&)=delete;Resident(Resident&&)=delete;
        std::string operator()(std::int64_t value)const{++calls;return std::to_string(value);}
    };
    struct BorrowedScope final {Resident resident;};
    auto borrowed_owner=std::make_shared<BorrowedScope>();auto borrowed_graph=std::make_shared<Bindings>();
    auto borrowed=make_callback_value<std::string(std::int64_t)>(borrowed_owner,borrowed_graph,
        [borrowed_owner](ReadContext&)->const Resident&{return borrowed_owner->resident;},
        [](const auto&,auto& handler,std::int64_t input){static_assert(std::is_const_v<std::remove_reference_t<decltype(handler)>>);return handler(input);});
    auto target_owner=std::make_shared<int>(0);auto target_graph=std::make_shared<Bindings>();
    auto borrowed_snapshot=snapshot_callback_value(target_owner,target_graph,borrowed);
    check((*borrowed_snapshot)(7)=="7"&&borrowed_owner->resident.calls==1,"A snapshot copied or changed a borrowed native callable");
    std::weak_ptr<BorrowedScope> borrowed_retired=borrowed_owner;
    check(bool(borrowed_graph->close()),"Borrowed source close failed");borrowed_owner.reset();
    check(borrowed_retired.expired(),"A frozen borrowed callable prolonged its source receiver lifetime");
    rejected=false;try{(void)(*borrowed_snapshot)(0);}catch(const std::logic_error&){rejected=true;}
    check(rejected&&bool(target_graph->close()),"A snapshot called a retired borrowed native object");
}
}
int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"results";
    if(name=="results")results();else if(name=="reactive")reactive();else if(name=="lifetime")lifetime();else if(name=="threading")threading();else if(name=="snapshot")snapshot();else if(name=="snapshot_lifetime")snapshot_lifetime();else throw std::runtime_error("Unknown function value test");
    std::cout<<"Native function value "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
