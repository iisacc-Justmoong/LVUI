#include <LVUI/NativeValues.h>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
using namespace lvui::native;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Scope final {
    ii::core::Observable<int> amount{1};
    int factories=0,total=0;
    std::function<void()> during;
};
struct Handler final {
    Scope* owner;
    std::unique_ptr<int> amount;
    std::shared_ptr<int> resource;
    void operator()(int value){if(owner->during)owner->during();owner->total+=*amount+value;}
};
auto make_action(const std::shared_ptr<Scope>& owner,const std::shared_ptr<Bindings>& bindings,std::weak_ptr<int>& resource){
    return returned_callback(owner,bindings,[owner,&resource](ReadContext& context){
        ++owner->factories;const auto amount=context.read(owner->amount);
        if(amount<0)throw std::runtime_error("Factory failed");
        auto token=std::make_shared<int>(0);resource=token;
        return Handler{owner.get(),std::make_unique<int>(amount),std::move(token)};
    },[](const auto&,Handler& handler,int value){handler(value);});
}
void reactive(){
    auto owner=std::make_shared<Scope>();auto graph=std::make_shared<Bindings>();std::weak_ptr<int> resource;
    auto action=make_action(owner,graph,resource);
    check(owner->factories==1&&owner->total==0,"A returned callable was deferred until the event or invoked while binding");
    action(2);check(owner->total==3&&owner->factories==1,"An event evaluated its factory again");
    owner->amount.set(4);check(owner->factories==2,"A changed callable dependency did not refresh the handler");
    action(1);check(owner->total==8,"The event used an obsolete returned handler");
    std::thread worker([&]{try{action(1);}catch(const std::logic_error&){}});worker.join();
    check(owner->total==8,"A foreign thread invoked an owner-thread returned callable");
}
void rollback(){
    auto owner=std::make_shared<Scope>();auto graph=std::make_shared<Bindings>();std::weak_ptr<int> resource;
    auto action=make_action(owner,graph,resource);owner->amount.set(-1);action(0);
    check(owner->total==1&&!graph->failures()->empty(),"A failed factory replaced the last valid callable");
    owner->amount.set(3);action(0);
    check(owner->total==4&&graph->failures()->empty(),"A returned callable did not recover after factory failure");
    auto failed=std::make_shared<Scope>();failed->amount.set(-1);bool threw=false;
    try{auto rejected=make_action(failed,std::make_shared<Bindings>(),resource);(void)rejected;}catch(const std::runtime_error&){threw=true;}
    check(threw,"Initial factory failure produced an apparently valid callback");
}
void lifetime(){
    auto owner=std::make_shared<Scope>();auto graph=std::make_shared<Bindings>();std::weak_ptr<int> resource;
    auto action=make_action(owner,graph,resource);std::weak_ptr<Scope> retired=owner;
    owner->during=[&]{owner.reset();check(bool(graph->close()),"Closing failed");check(!retired.expired()&&!resource.expired(),"An active returned callable lost its owner or resource");};
    action(1);
    check(retired.expired()&&resource.expired(),"An external saved callback retained a retired scope or returned resource");
    action(99);check(retired.expired(),"A late returned callback reopened a retired owner");
}
void typed(){
    auto unique=std::make_unique<int>(0);
    auto handler=[value=std::move(unique)](std::string text,std::int64_t count,bool checked,double number)mutable{
        check(text=="Native"&&count==7&&checked&&number==2.5,"Typed event adaptation changed its payload");++*value;
    };
    invoke_callback<void(std::string,std::int64_t,bool,double)>(handler,std::string_view("Native"),7,true,2.5f);
    struct Event final {std::unique_ptr<int> value=std::make_unique<int>(9);};
    const Event event;
    invoke_callback<void(Event)>([&](const Event& supplied){check(&supplied==&event&&*supplied.value==9,"A noncopyable C++ event was copied or replaced");},event);
    int overload=0;
    struct Overloaded final {int& value;void operator()(std::string){value=1;}void operator()(double){value=2;}} choose{overload};
    invoke_callback<void(std::string)>(choose,"Text");check(overload==1,"A callback overload ignored its declared String input");
    invoke_callback<void(double)>(choose,3);check(overload==2,"A callback overload ignored its declared numeric input");
    bool failed=false;try{invoke_callback<void(float)>([](float){},std::numeric_limits<double>::max());}catch(const std::out_of_range&){failed=true;}
    check(failed,"Narrowing a callback input silently overflowed its declared Float range");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"reactive";
    if(name=="reactive")reactive();else if(name=="rollback")rollback();else if(name=="lifetime")lifetime();else if(name=="typed")typed();else throw std::runtime_error("Unknown native callable test");
    std::cout<<"Native returned callback "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
