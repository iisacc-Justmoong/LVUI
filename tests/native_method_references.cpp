#include <LVUI/NativeMethods.h>
#include <iostream>
#include <stdexcept>
using namespace lvui::native;
namespace {
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Act final {
    template<class Object,class... Args>auto operator()(Object&& object,Args&&... args)const
        ->decltype(std::forward<Object>(object).act(std::forward<Args>(args)...)){
        return std::forward<Object>(object).act(std::forward<Args>(args)...);
    }
};
struct Item final {
    static inline int live=0;
    int calls=0,after=0;std::function<void()> during;
    Item(){++live;}~Item(){--live;}
    Item(const Item&)=delete;Item(Item&&)=delete;
    void act()&{++calls;if(auto action=std::exchange(during,{}))action();after=calls;}
};
struct Parent final {Item resident;};
class Owner final {
    ii::core::Observable<std::shared_ptr<Item>> selected_{std::make_shared<Item>()};
public:
    Item resident;
    ii::core::Observable<std::shared_ptr<Parent>> parent{std::make_shared<Parent>()};
    ii::core::Observable<int> file_revision{0};
    std::shared_ptr<Item> current()const{return selected_.get();}
    void select(std::shared_ptr<Item> item){selected_.set(std::move(item));}
};
auto make(const std::shared_ptr<Owner>& owner,const std::shared_ptr<Bindings>& graph){
    return make_callback_value<void()>(owner,graph,[owner](ReadContext& context){
        return Methods::reference(context,[owner](ReadContext&){return owner->current();},Act{});
    },[](const auto&,auto& handler){handler();});
}
void lvalues(){
    Item first,second;auto direct=Methods::reference([&](ReadContext&)->Item&{return first;},Act{});
    direct();Item* pointer=&first;
    auto raw=Methods::reference([&](ReadContext&){return pointer;},Act{});pointer=&second;raw();
    auto borrowed=Methods::reference([&](ReadContext&){return std::ref(first);},Act{});borrowed();
    check(first.calls==3&&second.calls==0,"A bound method copied its receiver or followed a changed raw handle");
    struct Const final {mutable int calls=0;void act()const{++calls;}};
    const Const value;auto immutable=Methods::reference([&](ReadContext&)->const Const&{return value;},Act{});immutable();
    check(value.calls==1,"A native method reference lost const receiver access");
    struct Rvalue final {int calls=0;Rvalue()=default;Rvalue(const Rvalue&)=delete;Rvalue(Rvalue&&)=delete;void act()&&{++calls;}} rvalue;
    auto forwarded=Methods::reference([&](ReadContext&)->Rvalue&&{return std::move(rvalue);},Act{});forwarded();
    check(rvalue.calls==1,"A borrowed rvalue reference was moved into a different receiver");
}
void selection(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto first=owner->current();
    auto action=make(owner,graph);check(first->calls==0,"Selecting a method reference invoked its method");
    (*action)();auto second=std::make_shared<Item>();owner->select(second);(*action)();
    check(first->calls==1&&second->calls==1&&owner->file_revision.get()==0,"A private receiver dependency was not selected without document changes");
    check(bool(graph->close()),"Selection graph close failed");
}
void failures(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto first=owner->current();auto action=make(owner,graph);
    owner->select({});(*action)();check(first->calls==1&&!graph->failures()->empty(),"A failed receiver selection replaced its last valid method");
    auto second=std::make_shared<Item>();owner->select(second);(*action)();
    check(second->calls==1&&graph->failures()->empty(),"A bound method did not recover after a live receiver returned");
    check(bool(graph->close()),"Failure graph close failed");
}
void initial(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();owner->select({});bool rejected=false;
    try{(void)make(owner,graph);}catch(const std::logic_error&){rejected=true;}
    auto stats=graph->stats();check(rejected&&stats&&stats->bindings==0&&stats->sources==0,"An invalid initial receiver left a partial callback binding");
    check(bool(graph->close()),"Initial failure graph close failed");
}
void inflight(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto action=make(owner,graph);
    std::weak_ptr<Item> previous=owner->current();auto next=std::make_shared<Item>();
    owner->current()->during=[&]{owner->select(next);check(!previous.expired(),"Replacing an active method destroyed its receiver");(*action)();};
    (*action)();check(previous.expired()&&next->calls==1,"Active and replacement receivers were confused or leaked");
    check(bool(graph->close()),"Active graph close failed");
}
void parent(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto action=make_callback_value<void()>(owner,graph,[owner](ReadContext& context){
        return Methods::reference(context,[owner](ReadContext& context)->Item&{
            return member(context,context.read(owner->parent),[](auto& parent)->Item&{return parent.resident;});
        },Act{});
    },[](const auto&,auto& handler){handler();});
    std::weak_ptr<Parent> previous=owner->parent.get();auto next=std::make_shared<Parent>();
    owner->parent.get()->resident.during=[&]{owner->parent.set(next);check(!previous.expired(),"A deferred subobject lost its selected shared parent");};
    (*action)();check(previous.expired(),"A replaced subobject reference retained its old parent");(*action)();check(next->resident.calls==1,"The subobject receiver did not change with its parent");
    check(bool(graph->close()),"Parent graph close failed");
}
struct Temporary final {
    static inline int live=0,calls=0;int count=0;
    Temporary(){++live;}~Temporary(){--live;}Temporary(const Temporary&)=delete;Temporary(Temporary&&)=delete;
    void act()&&{calls=++count;}
};
void temporary(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto action=make_callback_value<void()>(owner,graph,[](ReadContext& context){
        return Methods::reference(context,[](ReadContext&){return Temporary{};},Act{});
    },[](const auto&,auto& handler){handler();});
    check(Temporary::live==1&&Temporary::calls==0,"A temporary receiver was copied, discarded or invoked during selection");
    (*action)();(*action)();check(Temporary::calls==2,"An owned temporary lost identity or its rvalue-qualified method");
    const auto baseline=Item::live;
    auto ranged=make_callback_value<void()>(owner,graph,[](ReadContext& context){
        return Methods::reference(context,[](ReadContext& context)->decltype(auto){
            return Methods::receiver_index(context,[](ReadContext&){return std::vector<Item>(1);},[](ReadContext&){return 0;});
        },Act{});
    },[](const auto&,auto& handler){handler();});
    check(Item::live==baseline+1,"An indexed receiver lost its temporary noncopyable container");(*ranged)();
    check(bool(graph->close())&&Temporary::live==0&&Item::live==baseline,"A closed method value retained its temporary receiver or container");
}
void retirement(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto first=owner->current();
    auto action=make(owner,graph);auto pinned=snapshot_callback_value<void()>(owner,graph,action);auto next=std::make_shared<Item>();owner->select(next);
    (*action)();(*pinned)();check(first->calls==1&&next->calls==1,"A Static method reference followed a later receiver");
    std::weak_ptr<Owner> retired=owner;
    next->during=[&]{check(bool(graph->close()),"Retirement failed");owner.reset();check(!retired.expired(),"Self-removal destroyed an admitted receiver scope");};
    (*action)();check(retired.expired(),"A saved method value retained its retired activation");(*action)();(*pinned)();
    check(first->calls==1&&next->calls==2,"A retired method reference still admitted calls");
}
void threads(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto item=owner->current();auto action=make(owner,graph);
    std::thread worker([&]{(*action)();});worker.join();check(item->calls==0,"A foreign thread invoked a bound method receiver");
    check(bool(graph->close()),"Thread graph close failed");
}
}
int main(int argc,char** argv){try{
    std::string name=argc>1?argv[1]:"selection";
    if(name=="lvalues")lvalues();else if(name=="selection")selection();else if(name=="failures")failures();else if(name=="initial")initial();else if(name=="inflight")inflight();else if(name=="parent")parent();else if(name=="temporary")temporary();else if(name=="retirement")retirement();else if(name=="threads")threads();else throw std::runtime_error("Unknown native method reference test");
    check(Item::live==0&&Temporary::live==0,"Native method references leaked receivers");std::cout<<"Native method reference "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
