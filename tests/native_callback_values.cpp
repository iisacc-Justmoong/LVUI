#include <LVUI/NativeValues.h>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace lvui::native;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Owner final {ii::core::Observable<int> amount{1};int factories=0;std::vector<int> seen;std::function<void()> during;};
struct Handler final {
    Owner* owner;std::unique_ptr<int> state;std::shared_ptr<int> resource;
    void operator()(int extra){const auto value=++*state+extra;if(auto function=std::exchange(owner->during,{}))function();owner->seen.push_back(value);}
};
auto make(const std::shared_ptr<Owner>& owner,const std::shared_ptr<Bindings>& graph,std::weak_ptr<int>& resource){
    return make_callback_value<void(std::int64_t)>(owner,graph,[owner,&resource](ReadContext& context){
        ++owner->factories;const auto amount=context.read(owner->amount);if(amount<0)throw std::runtime_error("Shared factory failure");
        auto token=std::make_shared<int>(0);resource=token;return Handler{owner.get(),std::make_unique<int>(amount),std::move(token)};
    },[](const auto&,Handler& handler,std::int64_t value){handler(int(value));});
}
void identity(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();std::weak_ptr<int> resource;
    auto first=make(owner,graph,resource),second=first;check(owner->factories==1,"Creating consumers reevaluated their shared input");
    (*first)(0);(*second)(0);check(owner->seen==std::vector<int>{2,3},"Two consumers did not share a mutable callable value");
    auto old=resource;owner->amount.set(5);check(owner->factories==2&&first==second&&old.expired(),"Dependency replacement changed value identity or retained its old resource");
    (*second)(1);check(owner->seen.back()==7,"A consumer retained an obsolete callable version");
    struct Event final {std::unique_ptr<int> value=std::make_unique<int>(9);};
    const Event event;
    auto native_event=make_callback_value<void(Event)>(owner,graph,[](ReadContext&){return [](const Event& value){check(*value.value==9,"A native event payload was changed");};},
        [&event](const auto&,auto& handler,const Event& value){check(&value==&event,"A noncopyable event was copied by the shared value");handler(value);});
    (*native_event)(event);
    check(bool(graph->close())&&resource.expired(),"Closing did not release the current shared callable");
    (*first)(99);check(owner->seen.back()==7,"A saved callback value invoked a retired handler");
}
void rollback(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();std::weak_ptr<int> resource;
    auto value=make(owner,graph,resource);owner->amount.set(-1);(*value)(0);
    check(owner->seen==std::vector<int>{2}&&!graph->failures()->empty(),"A failed factory replaced the shared value");
    owner->amount.set(3);(*value)(0);check(owner->seen.back()==4&&graph->failures()->empty(),"A shared value failed to recover");
    check(bool(graph->close()),"Shared value close failed");
}
void active(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();std::weak_ptr<int> resource;
    auto value=make(owner,graph,resource);auto old=resource;
    owner->during=[&]{owner->amount.set(10);check(!old.expired(),"Replacing an active handler destroyed its capture");(*value)(0);};
    (*value)(0);check(owner->seen==std::vector<int>{11,2}&&old.expired(),"Active and replacement callable versions were confused");
    std::weak_ptr<Owner> retired=owner;
    owner->during=[&]{value.reset();owner.reset();check(bool(graph->close()),"Self-removing value close failed");check(!retired.expired()&&!resource.expired(),"Self-removal destroyed active resources");};
    (*value)(0);check(retired.expired()&&resource.expired(),"An active value retained its receiver after returning");
}
void threading(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();std::weak_ptr<int> resource;
    auto value=make(owner,graph,resource);
    std::thread worker([&]{(*value)(7);});worker.join();
    check(owner->seen.empty(),"A foreign thread invoked the shared native receiver");
    check(bool(graph->close()),"Shared value close failed");
}
int free_value=0;
void free_handler(std::int64_t value){free_value+=int(value);}
void native_results(){
    struct Native final {
        mutable int calls=0;
        Native()=default;Native(const Native&)=delete;Native(Native&&)=delete;
        void operator()()const{++calls;}
    };
    struct Scope final {Native resident;};
    auto owner=std::make_shared<Scope>();auto graph=std::make_shared<Bindings>();
    auto owned=make_callback_value<void()>(owner,graph,[](ReadContext&){return Native{};},[](const auto&,Native& handler){handler();});
    (*owned)();
    auto borrowed=make_callback_value<void()>(owner,graph,[owner](ReadContext&)->Native&{return owner->resident;},
        [](const auto& owner,Native& handler){check(&handler==&owner->resident,"A C++ lvalue callable was copied");handler();});
    auto immutable=make_callback_value<void()>(owner,graph,[owner](ReadContext&)->const Native&{return owner->resident;},
        [](const auto&,auto& handler){static_assert(std::is_const_v<std::remove_reference_t<decltype(handler)>>);handler();});
    (*borrowed)();(*immutable)();check(owner->resident.calls==2,"Borrowed constness or native callable identity was lost");
    auto function=make_callback_value<void(std::int64_t)>(owner,graph,[](ReadContext&)->decltype(free_handler)&{return free_handler;},
        [](const auto&,auto& handler,std::int64_t value){handler(value);});
    (*function)(7);check(free_value==7,"A C++ function reference was rejected or copied incorrectly");
    check(bool(graph->close()),"Native result close failed");
    std::weak_ptr<Scope> retired=owner;owner.reset();(*borrowed)();check(retired.expired(),"A saved borrowed callable retained its native owner");
}
void returned_results(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto function=returned_function<ii::core::Result<int>>(owner,graph,[owner](ReadContext& context){
        const auto amount=context.read(owner->amount);if(amount<0)throw std::runtime_error("Result factory failure");
        return [amount](int value)->ii::core::Result<int>{return amount+value;};
    },[](const auto&,auto& handler,int value)->ii::core::Result<int>{return handler(value);});
    auto first=function(2);check(first&&*first==3,"Returned callable lost its result");
    owner->amount.set(5);auto updated=function(2);check(updated&&*updated==7,"Returned result did not use its current handler");
    owner->amount.set(-1);auto retained=function(2);check(retained&&*retained==7&&!graph->failures()->empty(),"Failed result factory replaced its valid handler");
    bool foreign=false;std::thread worker([&]{try{(void)function(1);}catch(const std::logic_error&){foreign=true;}});worker.join();
    check(foreign,"Foreign thread invoked a returned result function");
    check(bool(graph->close()),"Result callable close failed");bool closed=false;
    try{(void)function(1);}catch(const std::logic_error&){closed=true;}check(closed,"Retired result function remained callable");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"identity";
    if(name=="identity")identity();else if(name=="rollback")rollback();else if(name=="active")active();else if(name=="threading")threading();else if(name=="native_results")native_results();else if(name=="returned_results")returned_results();else throw std::runtime_error("Unknown shared callback value test");
    std::cout<<"Native callback value "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
