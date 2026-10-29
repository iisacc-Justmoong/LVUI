#include <LVUI/NativeMethods.h>
#include "fixtures/NativeFactoryCaptureFixture.h"
#include <iostream>
#include <thread>
using namespace lvui::native;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
using Owner=NativeFactoryCaptureBackend;
auto make(const std::shared_ptr<Owner>& owner,const std::shared_ptr<Bindings>& graph){
    return make_callback_value<void()>(owner,graph,[owner](ReadContext& context){
        return Methods::callable(context,[owner](ReadContext& context)->decltype(auto){
            return Methods::receiver_member(context,[owner](ReadContext& context)->decltype(auto){return context.read(owner->selected);},
                [](auto& node)->decltype(auto){return node.make();});
        });
    },[](const auto&,auto& selected){selected();});
}
void receiver(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto previous=std::make_shared<FactoryCaptureDomain::Node>("Independent");owner->selected.set(previous);auto action=make(owner,graph);
    std::weak_ptr<FactoryCaptureDomain::Node> old=previous;auto next=std::make_shared<FactoryCaptureDomain::Node>("Next");
    previous->during=[&]{owner->selected.set(next);check(!old.expired(),"Replacing a returned callback destroyed its active receiver");(*action)();};previous.reset();
    (*action)();check(old.expired()&&next->count.get()==1&&owner->file_revision.get()==0,"A returned callback receiver leaked or reentry used the old version");
    check(bool(graph->close()),"Receiver graph close failed");
}
void temporary(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();const auto before=FactoryCaptureDomain::Node::live;
    auto action=make_callback_value<void()>(owner,graph,[owner](ReadContext& context){
        return Methods::callable(context,[owner](ReadContext& context)->decltype(auto){
            return Methods::receiver_member(context,[owner](ReadContext&){return owner->temporary();},[](auto&& node)->decltype(auto){return node.make();});
        });
    },[](const auto&,auto& selected){selected();});
    check(FactoryCaptureDomain::Node::live==before+1,"A borrowed factory result lost its immovable temporary receiver");(*action)();
    check(bool(graph->close())&&FactoryCaptureDomain::Node::live==before,"A retired factory retained its temporary receiver");
}
void argument(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto action=make_callback_value<void()>(owner,graph,[owner](ReadContext& context){
        return Methods::callable(context,[owner](ReadContext& context){
            return owner->capture(Methods::argument(context,[owner](ReadContext&){return owner->argument();}));
        });
    },[](const auto&,auto& selected){selected();});
    check(FactoryCaptureDomain::Argument::live==1,"A returned callable lost its immovable argument");(*action)();
    check(owner->last=="Argument","A retained argument lost its contents");
    check(bool(graph->close())&&FactoryCaptureDomain::Argument::live==0,"A retired callable retained its argument");
}
void owned(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto action=make_callback_value<void()>(owner,graph,[owner](ReadContext& context){
        return Methods::callable(context,[owner](ReadContext& context){
            return Methods::receiver_member(context,[owner](ReadContext& context)->decltype(auto){return context.read(owner->selected);},[](auto& node){return node.value();});
        });
    },[](const auto&,auto& selected){selected();});
    (*action)();(*action)();check(owner->first->count.get()==2,"An owned immovable factory result lost lvalue invocation or identity");
    check(bool(graph->close()),"Owned graph close failed");
}
void adaptive(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto action=make_callback_value<void()>(owner,graph,[owner](ReadContext& context){
        return Methods::callable(context,[owner](ReadContext& context){
            auto first=[owner](ReadContext&){return owner->argument();};auto second=first;
            constexpr auto retained=Methods::arguments<2>([]<auto selected>(){
                return requires{std::declval<Owner&>().mixed(
                    Methods::argument_value<selected[0]>(std::declval<ReadContext&>(),std::declval<decltype(first)&>()),
                    Methods::argument_value<selected[1]>(std::declval<ReadContext&>(),std::declval<decltype(second)&>()));};
            });
            static_assert(retained[0]&&!retained[1]);
            return owner->mixed(Methods::argument_value<retained[0]>(context,first),Methods::argument_value<retained[1]>(context,second));
        });
    },[](const auto&,auto& selected){selected();});
    check(FactoryCaptureDomain::Argument::live==1,"Native argument selection discarded borrowed storage or retained a by-value parameter");(*action)();
    check(owner->last=="Argument/Argument","Mixed native arguments lost mandatory elision or reference identity");
    check(bool(graph->close())&&FactoryCaptureDomain::Argument::live==0,"Mixed native arguments leaked storage");
}
void failures(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto action=make(owner,graph);
    owner->selected.set({});(*action)();check(owner->first->count.get()==1&&!graph->failures()->empty(),"A failed factory discarded the last valid callable or dependency");
    owner->selected.set(owner->second);(*action)();check(owner->second->count.get()==1&&graph->failures()->empty(),"A captured factory did not recover");
    check(bool(graph->close()),"Failure graph close failed");
    owner->selected.set({});auto invalid=std::make_shared<Bindings>();bool rejected=false;try{(void)make(owner,invalid);}catch(const std::logic_error&){rejected=true;}
    auto stats=invalid->stats();check(rejected&&stats&&stats->bindings==0&&stats->sources==0,"An invalid initial factory left a partial binding");check(bool(invalid->close()),"Initial failure graph close failed");
}
void frozen(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto action=make(owner,graph);auto pinned=snapshot_callback_value(owner,graph,action);
    owner->selected.set(owner->second);(*pinned)();(*action)();check(owner->first->count.get()==1&&owner->second->count.get()==1,"A factory snapshot followed a new receiver");
    check(bool(graph->close()),"Frozen graph close failed");
}
void categories(){
    struct Token final {Token()=default;Token(const Token&)=delete;Token(Token&&)=delete;};
    struct Const final {mutable int calls=0;void operator()()const{++calls;}};
    struct Local final {Token token;Const function;};auto owner=std::make_shared<Local>();auto graph=std::make_shared<Bindings>();
    auto action=make_callback_value<void()>(owner,graph,[owner](ReadContext& context){
        return Methods::callable(context,[owner](ReadContext& context)->const Const&{
            auto&& left=Methods::argument(context,[owner](ReadContext&)->Token&{return owner->token;});
            auto&& right=Methods::argument(context,[owner](ReadContext&)->Token&&{return std::move(owner->token);});
            static_assert(std::is_same_v<decltype(left),Token&>&&std::is_same_v<decltype(right),Token&&>);
            check(&left==&owner->token&&&right==&owner->token,"Native argument references were moved or cloned");return owner->function;
        });
    },[](const auto&,auto& selected){selected();});
    (*action)();check(owner->function.calls==1,"A const borrowed factory result was copied or lost const access");
    check(bool(graph->close()),"Category graph close failed");
}
void retirement(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto action=make(owner,graph);std::weak_ptr<Owner> retired=owner;
    owner->first->during=[&]{owner.reset();check(bool(graph->close()),"Self-removal graph close failed");check(!retired.expired(),"Self-removal destroyed an admitted factory owner");};
    (*action)();check(retired.expired(),"An external factory value retained its retired owner");(*action)();
}
void threads(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();auto action=make(owner,graph);
    std::thread thread([&]{(*action)();});thread.join();check(owner->first->count.get()==0,"A foreign execution domain invoked a factory result");
    check(bool(graph->close()),"Thread graph close failed");
}
}
int main(int argc,char** argv){try{
    const std::string name=argc>1?argv[1]:"receiver";
    if(name=="receiver")receiver();else if(name=="temporary")temporary();else if(name=="argument")argument();else if(name=="adaptive")adaptive();else if(name=="owned")owned();else if(name=="failures")failures();else if(name=="frozen")frozen();else if(name=="categories")categories();else if(name=="retirement")retirement();else if(name=="threads")threads();else throw std::runtime_error("Unknown native factory capture test");
    check(Owner::live==0&&FactoryCaptureDomain::Node::live==0&&FactoryCaptureDomain::Argument::live==0,"Native callable factories leaked resources");std::cout<<"Native factory capture "<<name<<" passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
