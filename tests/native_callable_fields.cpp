#include <LVUI/NativeMethods.h>
#include <iostream>
#include <thread>
using namespace lvui::native;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Call final {
    template<class Object,class... Args>auto operator()(Object&& object,Args&&... args)const
        ->decltype(std::forward<Object>(object).run(std::forward<Args>(args)...)){
        return std::forward<Object>(object).run(std::forward<Args>(args)...);
    }
};
struct Field final {
    template<class Object>auto operator()(Object&& object)const ->decltype((std::forward<Object>(object).run)){
        return (std::forward<Object>(object).run);
    }
};
struct Owner final {
    ii::core::Observable<std::function<int()>> run{[count=0]()mutable{return ++count;}};
    ii::core::Observable<int> file_revision{0};
};
auto value(const std::shared_ptr<Owner>& owner,const std::shared_ptr<Bindings>& graph){
    return make_callback_value<std::int64_t()>(owner,graph,[owner](ReadContext& context){
        return Methods::reference(context,[owner](ReadContext&)->Owner&{return *owner;},Call{},Field{});
    },[](const auto&,auto& selected){return selected();});
}
void versions(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto current=value(owner,graph);
    auto frozen=snapshot_callback_value(owner,graph,current);auto another=current;
    check((*current)()==1&&(*another)()==2,"Consumers did not share the selected callable field version");
    owner->run.set([count=10]()mutable{return ++count;});
    check((*current)()==11&&(*frozen)()==3&&owner->file_revision.get()==0,"Observable field replacement changed a frozen version or document state");
    check(bool(graph->close()),"Version graph close failed");
}
void errors(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto current=value(owner,graph);
    owner->run.set({});check((*current)()==1&&!graph->failures()->empty(),"An empty published field discarded its last valid callable");
    owner->run.set([]{return 7;});check((*current)()==7&&graph->failures()->empty(),"A failed callable field did not recover");
    check(bool(graph->close()),"Error graph close failed");
}
void initial(){
    auto owner=std::make_shared<Owner>();owner->run.set({});auto graph=std::make_shared<Bindings>();bool rejected=false;
    try{(void)value(owner,graph);}catch(const std::invalid_argument&){rejected=true;}
    auto stats=graph->stats();check(rejected&&stats&&stats->bindings==0&&stats->sources==0,"An empty initial field left a partial binding");
    check(bool(graph->close()),"Initial graph close failed");
}
void publication(){
    struct Readable final {
        std::function<int()> value=[]{return 1;};std::uint64_t version=0;
        const auto& get()const{return value;}std::uint64_t revision()const{return version;}
        void set(std::function<int()> next){value=std::move(next);++version;}
    };
    struct Model final {Readable run;ii::core::Observable<std::uint64_t> revision{0};};
    auto owner=std::make_shared<Model>();auto graph=std::make_shared<Bindings>();
    auto current=make_callback_value<std::int64_t()>(owner,graph,[owner](ReadContext& context){
        return Methods::reference(context,[owner](ReadContext&)->Model&{return *owner;},Call{},Field{},
            [owner](ReadContext& context){(void)context.read(owner->revision);});
    },[](const auto&,auto& selected){return selected();});
    check((*current)()==1,"A readable callable did not select its initial value");
    owner->run.set([]{return 2;});owner->revision.set(1);
    check((*current)()==2,"A non-observable readable field ignored its owner publication");
    owner->run.set({});owner->revision.set(2);check((*current)()==2&&!graph->failures()->empty(),"A failed readable field discarded its last version");
    owner->run.set([]{return 3;});owner->revision.set(3);check((*current)()==3&&graph->failures()->empty(),"A readable field did not recover through owner publication");
    check(bool(graph->close()),"Readable publication graph close failed");
}
struct Handler final {
    static inline int live=0;
    std::unique_ptr<int> count;std::function<void()> during;int after=0;
    explicit Handler(int n):count(std::make_unique<int>(n)){++live;}~Handler(){--live;}
    Handler(const Handler&)=delete;Handler(Handler&&)=delete;
    int operator()()&{const auto result=++*count;if(auto action=std::exchange(during,{}))action();after=result;return result;}
};
struct SharedOwner final {ii::core::Observable<std::shared_ptr<Handler>> run{std::make_shared<Handler>(0)};};
auto shared_value(const std::shared_ptr<SharedOwner>& owner,const std::shared_ptr<Bindings>& graph){
    return make_callback_value<std::int64_t()>(owner,graph,[owner](ReadContext& context){
        return Methods::reference(context,[owner](ReadContext&)->SharedOwner&{return *owner;},Call{},Field{});
    },[](const auto&,auto& selected){return selected();});
}
void inflight(){
    auto copied_owner=std::make_shared<Owner>();auto copied_graph=std::make_shared<Bindings>();
    auto token=std::make_shared<int>(8);std::weak_ptr<int> copied_capture=token;
    copied_owner->run.set([&copied_owner,copied_capture,token]{
        copied_owner->run.set([]{return 20;});check(!copied_capture.expired(),"Replacement destroyed an active copied callable capture");return *token;
    });token.reset();auto copied=value(copied_owner,copied_graph);
    check((*copied)()==8&&copied_capture.expired()&&(*copied)()==20,"An active copied field version changed or retained its capture");
    check(bool(copied_graph->close()),"Copied active graph close failed");
    auto owner=std::make_shared<SharedOwner>();auto graph=std::make_shared<Bindings>();auto current=shared_value(owner,graph);
    std::weak_ptr<Handler> previous=owner->run.get();auto next=std::make_shared<Handler>(10);
    owner->run.get()->during=[&]{owner->run.set(next);check(!previous.expired(),"Replacement destroyed an active callable field");check((*current)()==11,"Reentry used the previous field version");};
    check((*current)()==1&&previous.expired(),"An admitted callable version changed or leaked");
    owner->run.set({});check((*current)()==12&&!graph->failures()->empty(),"A null shared field replaced the last valid version");
    owner->run.set(std::make_shared<Handler>(20));check((*current)()==21&&graph->failures()->empty(),"A null callable did not recover");
    check(bool(graph->close()),"Active graph close failed");
}
void borrowed(){
    struct Const final {mutable int calls=0;void operator()()const{++calls;}};
    struct Model final {Handler run{0};Const immutable;};
    auto owner=std::make_shared<Model>();auto graph=std::make_shared<Bindings>();
    auto current=make_callback_value<std::int64_t()>(owner,graph,[owner](ReadContext& context){
        return Methods::reference(context,[owner](ReadContext&)->Model&{return *owner;},Call{},Field{});
    },[](const auto&,auto& selected){return selected();});
    check((*current)()==1&&(*current)()==2&&*owner->run.count==2,"A borrowed noncopyable callable lost its C++ identity");
    struct ReferenceModel final {std::reference_wrapper<const Const> run;};ReferenceModel model{owner->immutable};
    auto constant=make_callback_value<void()>(owner,graph,[&model](ReadContext& context){
        return Methods::reference(context,[&model](ReadContext&)->ReferenceModel&{return model;},Call{},Field{});
    },[](const auto&,auto& selected){selected();});
    (*constant)();check(owner->immutable.calls==1,"A borrowed callable lost const access");
    check(bool(graph->close()),"Borrowed graph close failed");
}
int first_calls=0,second_calls=0;
void first(){++first_calls;}void second(){++second_calls;}
void handles(){
    struct Model final {void(*run)()=&first;};auto owner=std::make_shared<Model>();auto graph=std::make_shared<Bindings>();
    auto current=make_callback_value<void()>(owner,graph,[owner](ReadContext& context){
        return Methods::reference(context,[owner](ReadContext&)->Model&{return *owner;},Call{},Field{});
    },[](const auto&,auto& selected){selected();});
    owner->run=&second;(*current)();check(first_calls==1&&second_calls==0,"A selected function pointer followed a replaced field handle");
    check(bool(graph->close()),"Handle graph close failed");
}
struct Temporary final {
    static inline int live=0,calls=0;
    Temporary(){++live;}~Temporary(){--live;}Temporary(const Temporary&)=delete;Temporary(Temporary&&)=delete;
    void operator()()&&{++calls;}
};
void accessors(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto current=make_callback_value<void()>(owner,graph,[owner](ReadContext& context){
        return Methods::reference(context,[owner](ReadContext&)->Owner&{return *owner;},Call{},[](auto&){return Temporary{};});
    },[](const auto&,auto& selected){selected();});
    check(Temporary::live==1&&Temporary::calls==0,"A field accessor discarded its immovable temporary callable");
    (*current)();(*current)();check(Temporary::calls==2,"An owned field accessor lost its callable identity or rvalue category");
    check(bool(graph->close())&&Temporary::live==0,"A retired field accessor retained its temporary callable");
}
void retirement(){
    auto owner=std::make_shared<SharedOwner>();auto graph=std::make_shared<Bindings>();auto current=shared_value(owner,graph);
    auto frozen=snapshot_callback_value(owner,graph,current);std::weak_ptr<SharedOwner> retired=owner;std::weak_ptr<Handler> selected=owner->run.get();
    owner->run.get()->during=[&]{owner.reset();check(bool(graph->close()),"Self-removal close failed");check(!retired.expired()&&!selected.expired(),"Self-removal destroyed an admitted field or receiver");};
    check((*current)()==1&&retired.expired()&&selected.expired(),"An external field value retained a retired scope");
    bool rejected=false;try{(void)(*frozen)();}catch(const std::logic_error&){rejected=true;}check(rejected,"A retired frozen field accepted a new call");
}
void threads(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto current=value(owner,graph);bool rejected=false;
    std::thread thread([&]{try{(void)(*current)();}catch(const std::logic_error&){rejected=true;}});thread.join();
    check(rejected&&(*current)()==1,"A foreign domain executed or changed the field version");check(bool(graph->close()),"Thread graph close failed");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"versions";
    if(name=="versions")versions();else if(name=="errors")errors();else if(name=="initial")initial();else if(name=="publication")publication();else if(name=="inflight")inflight();
    else if(name=="borrowed")borrowed();else if(name=="handles")handles();else if(name=="accessors")accessors();else if(name=="retirement")retirement();else if(name=="threads")threads();
    else throw std::runtime_error("Unknown native callable field test");
    check(Handler::live==0,"Callable fields leaked native resources");std::cout<<"Native callable field "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
