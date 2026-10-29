#include <LVUI/NativeMethods.h>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
class Item final {
    ii::core::Observable<int> count_{3},alternate_{8};
    ii::core::Observable<bool> use_alternate_{false};
public:
    mutable int calls=0;mutable std::function<void()> on_read;
    std::string title="Native";
    int value()const{++calls;const auto value=use_alternate_.get()?alternate_.get():count_.get();if(auto action=std::exchange(on_read,{}))action();if(value<0)throw std::runtime_error("invalid value");return value;}
    std::string_view text()const{if(auto action=std::exchange(on_read,{}))action();return title;}
    void set(int value){count_.set(value);}void alternate(int value){alternate_.set(value);}void select(bool alternate){use_alternate_.set(alternate);}
};
void private_reads(){
    Item item;lvui::native::Bindings graph;int shown=0;
    auto binding=graph.bind([&](auto& context){return lvui::native::Methods::query(context,item,[](const auto& receiver){return receiver.value();});},[&](int value)->ii::core::Result<void>{shown=value;return {};});
    check(binding&&shown==3,"A native getter failed initially");item.set(5);check(shown==5,"Private C++ Observable reads were not collected");
    item.select(true);check(shown==8,"A getter branch did not change its dependencies");auto calls=item.calls;item.set(9);check(shown==8&&item.calls==calls,"An inactive getter branch still caused reevaluation");item.alternate(11);check(shown==11,"A replacement getter branch stopped observing");
}
void failures(){
    Item item;lvui::native::Bindings graph;int shown=0;auto binding=graph.bind([&](auto& context){return lvui::native::Methods::evaluate(context,[&]{return item.value();});},[&](int value)->ii::core::Result<void>{shown=value;return {};});
    check(bool(binding),"Method binding failed");item.set(-1);check(shown==3&&graph.failures()->size()==1,"A failed C++ query erased valid state or lost its error");item.set(7);check(shown==7&&graph.failures()->empty(),"C++ query dependencies did not permit recovery");
}
void canonical(){
    ii::core::Observable<int> source{2};lvui::native::Bindings graph;int shown=0;
    auto binding=graph.bind([&](auto& context){const auto value=context.read(source);return lvui::native::Methods::evaluate(context,[&]{return source.get()+value;});},[&](int value)->ii::core::Result<void>{shown=value;return {};});
    check(binding&&graph.stats()->sources==1&&shown==4,"Explicit and method-internal reads created duplicate source identities");source.set(4);check(shown==8&&graph.stats()->sources==1,"Canonical method source identity changed after publication");
}
void reentry(){
    auto first=std::make_shared<Item>();std::weak_ptr<Item> previous=first;auto second=std::make_shared<Item>();second->set(9);
    ii::core::Observable<std::shared_ptr<Item>> selected{first};lvui::native::Bindings graph;int shown=0;
    first->on_read=[&]{selected.set(second);first.reset();};
    auto binding=graph.bind([&](auto& context){return lvui::native::Methods::query(context,context.read(selected),[](const auto& receiver){return receiver.value();});},[&](int value)->ii::core::Result<void>{shown=value;return {};});
    check(binding&&shown==9&&previous.expired(),"Reentrant receiver replacement lost source stability or retained the obsolete receiver");
}
void initial(){
    auto first=std::make_shared<Item>();std::weak_ptr<Item> previous=first;ii::core::Observable<std::shared_ptr<Item>> selected{first};
    first->on_read=[&]{selected.set({});first.reset();};
    auto result=lvui::native::Methods::evaluate([&]{return selected.get()->text();});
    check(result=="Native"&&previous.expired(),"An initial borrowed string outlived its C++ receiver before ownership conversion");
    bool invalid=false;try{(void)lvui::native::Methods::evaluate([]()->const char*{return nullptr;});}catch(const std::invalid_argument&){invalid=true;}
    check(invalid,"A null native string entered an initial query without validation");
}
void isolation(){
    Item item;ii::core::Observable<int> trigger{0},unrelated{4};lvui::native::Bindings other,graph;
    auto separate=other.bind([&](auto& context){(void)context.read(trigger);return unrelated.get();},[](int)->ii::core::Result<void>{return {};});check(bool(separate),"Independent graph failed");
    int calls=0;auto binding=graph.bind([&](auto& context){return lvui::native::Methods::evaluate(context,[&]{trigger.set(++calls);return item.value();});},[](int)->ii::core::Result<void>{return {};});
    // Reads made by the independently triggered graph do not belong to this query.
    check(bool(binding),"Reentrant graph query failed");auto before=calls;unrelated.set(8);check(calls==before,"An independent graph contaminated an outer method's read set");
}
int main(int argc,char** argv){try{std::string name=argc>1?argv[1]:"private";if(name=="private")private_reads();else if(name=="failures")failures();else if(name=="canonical")canonical();else if(name=="reentry")reentry();else if(name=="initial")initial();else if(name=="isolation")isolation();else throw std::runtime_error("Unknown native method test");std::cout<<"Native methods "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
