#include <LVUI/NativeInferredValues.h>
#include <iostream>
using namespace lvui::native;
static_assert(std::invocable<CallbackValue<void()>&>);
static_assert(!std::invocable<CallbackValue<void()>&,std::string>);
static_assert(!std::invocable<CallbackValue<void(std::string)>&>);
static_assert(std::invocable<CallbackValue<void(std::string)>&,std::string>);
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Owner final {
    ii::core::Observable<int> count{2};
    ii::core::Observable<std::uint64_t> revision{0};
    std::string format(std::int64_t value)const{return std::to_string(count.get()+value);}
    auto make(){return [offset=std::make_unique<int>(count.get())](std::int64_t value){return std::to_string(*offset+value);};}
};
auto method(ReadContext& context,const std::shared_ptr<Owner>& owner){
    return Methods::inferred_member(context,[owner](ReadContext&)->Owner&{return *owner;},
        [](auto& value)->decltype((value.format)){return (value.format);},
        [](auto& value,auto&&... args)->decltype(value.format(std::forward<decltype(args)>(args)...)){return value.format(std::forward<decltype(args)>(args)...);},
        [](auto& value)->decltype(&std::remove_cvref_t<decltype(value)>::format){return &std::remove_cvref_t<decltype(value)>::format;},
        [owner](ReadContext& context){(void)context.read(owner->revision);},inferred::Marker{});
}
auto initial_method(const std::shared_ptr<Owner>& owner){
    return Methods::inferred_member([owner](ReadContext&)->Owner&{return *owner;},
        [](auto& value)->decltype((value.format)){return (value.format);},
        [](auto& value,auto&&... args)->decltype(value.format(std::forward<decltype(args)>(args)...)){return value.format(std::forward<decltype(args)>(args)...);},
        [](auto& value)->decltype(&std::remove_cvref_t<decltype(value)>::format){return &std::remove_cvref_t<decltype(value)>::format;},inferred::Marker{});
}
void scalar(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto make_definition=[owner](ReadContext& context){return Methods::inferred_result(context,[owner](ReadContext& context){return context.read(owner->count);},inferred::Marker{});};
    auto initial=[owner]{return Methods::inferred_result([owner](ReadContext&){return owner->count.get();},inferred::Marker{});};
    auto current=inferred::make<inferred::Mode::readonly>(owner,graph,initial,make_definition);
    auto fixed=inferred::make<inferred::Mode::fixed>(owner,graph,initial,make_definition);
    auto variable=inferred::make<inferred::Mode::variable>(owner,graph,initial,make_definition);
    check(current->get()==2&&fixed->get()==2&&variable->get()==2,"Inferred scalar initialization failed");
    owner->count.set(4);check(current->get()==4&&fixed->get()==2&&variable->get()==2,"Inferred scalar storage modes were confused");
    check(bool(variable->set(8))&&variable->get()==8,"Inferred scalar assignment failed");check(bool(graph->close()),"Scalar graph close failed");
}
void methods(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto value=inferred::make<inferred::Mode::readonly>(owner,graph,[owner]{return initial_method(owner);},[owner](ReadContext& context){return method(context,owner);});
    check(inferred::call(*value,3)=="5","An inferred C++ method lost its signature or result");
    owner->count.set(8);check(inferred::call(*value,3)=="11","An inferred method lost its native receiver");check(bool(graph->close()),"Method graph close failed");
}
void factories(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto initial=[owner]{return Methods::inferred_result([owner](ReadContext&){return owner->make();},inferred::Marker{});};
    auto factory=[owner](ReadContext& context){return Methods::inferred_result(context,[owner](ReadContext& context){(void)context.read(owner->count);return owner->make();},inferred::Marker{});};
    auto value=inferred::make<inferred::Mode::readonly>(owner,graph,initial,factory);
    std::uint64_t changes=0;auto watch=value->observe([&](const auto&,std::uint64_t){++changes;});
    check(inferred::call(*value,1)=="3","An inferred move-only factory result failed");owner->count.set(5);
    check(inferred::call(*value,1)=="6"&&changes==1&&value->revision()==1,"Inferred function versions were not observable");check(bool(graph->close()),"Factory graph close failed");
}
void aliases(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto initial=[owner]{return Methods::inferred_result([owner](ReadContext&){return owner->make();},inferred::Marker{});};
    auto formula=[owner](ReadContext& context){return Methods::inferred_result(context,[owner](ReadContext& context){(void)context.read(owner->count);return owner->make();},inferred::Marker{});};
    auto source=inferred::make<inferred::Mode::readonly>(owner,graph,initial,formula);
    auto first=[source]{return Methods::inferred_result([source](ReadContext&){return source->get();},inferred::Marker{});};
    auto next=[source](ReadContext& context){return Methods::inferred_result(context,[source](ReadContext& context){return context.read(*source);},inferred::Marker{});};
    auto alias=inferred::make<inferred::Mode::readonly>(owner,graph,first,next);
    auto fixed=inferred::make<inferred::Mode::fixed>(owner,graph,first,next);
    auto variable=inferred::make<inferred::Mode::variable>(owner,graph,first,next);
    owner->count.set(7);check(inferred::call(*alias,1)=="8"&&inferred::call(*fixed,1)=="3"&&inferred::call(*variable,1)=="3","Inferred aliases did not preserve reactive and selected versions");
    check(bool(variable->set(alias->get()))&&inferred::call(*variable,1)=="8","Inferred function assignment failed");check(bool(graph->close()),"Alias graph close failed");
}
}
void observation(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto initial=[owner]{return Methods::inferred_result([owner](ReadContext&){return owner->make();},inferred::Marker{});};
    auto formula=[owner](ReadContext& context){return Methods::inferred_result(context,[owner](ReadContext& context){(void)context.read(owner->count);return owner->make();},inferred::Marker{});};
    auto value=inferred::make<inferred::Mode::readonly>(owner,graph,initial,formula);
    auto function=value->get();std::uint64_t changes=0;
    std::optional<ii::core::ObservableSubscription> watch;
    watch.emplace(function->observe([&](std::uint64_t revision){++changes;check(revision==1&&(*function)(1)=="6","Function observation preceded selection commit");watch.reset();}));
    owner->count.set(5);owner->count.set(7);
    check(changes==1&&function->revision()==2,"Function observation removal or version advancement failed");
    bool rejected=false;std::thread wrong([&]{try{(void)function->revision();}catch(const std::logic_error&){rejected=true;}});wrong.join();
    check(rejected,"Function versions were observable outside the owner thread");check(bool(graph->close()),"Observed graph close failed");
    rejected=false;try{(void)function->select();}catch(const std::logic_error&){rejected=true;}check(rejected,"A retired function admitted a new selected version");
}
void reentry(){
    auto owner=std::make_shared<Owner>();auto graph=std::make_shared<Bindings>();
    auto initial=[owner]{return Methods::inferred_result([owner](ReadContext&){return owner->make();},inferred::Marker{});};
    auto formula=[owner](ReadContext& context){return Methods::inferred_result(context,[owner](ReadContext& context){(void)context.read(owner->count);return owner->make();},inferred::Marker{});};
    auto value=inferred::make<inferred::Mode::readonly>(owner,graph,std::move(initial),std::move(formula));
    auto function=value->get();std::weak_ptr<Owner> old=owner;
    auto watch=function->observe([&](std::uint64_t){check((*function)(1)=="6","Reentrant invocation used the previous inferred function");check(bool(graph->close()),"Reentrant retirement failed");value.reset();owner.reset();});
    auto admitted=owner;admitted->count.set(5);admitted.reset();
    check(old.expired(),"Function version subscriptions retained a retired native owner");
    bool rejected=false;try{(void)(*function)(1);}catch(const std::logic_error&){rejected=true;}check(rejected,"A retired inferred function accepted a value call");
}
void maps(){
    using V=ii::core::Value;auto source=std::make_shared<ii::core::Observable<V>>(V(V::Object{{"amount",V(2)}}));auto graph=std::make_shared<Bindings>();
    auto factory=[source](ReadContext& context){return context.read(*source);};
    auto field=[](auto&& object)->decltype((object.amount)){return (object.amount);};
    auto call=[](auto&& object,auto&&... args)->decltype(object.amount(std::forward<decltype(args)>(args)...)){return object.amount(std::forward<decltype(args)>(args)...);};
    auto pointer=[](auto&& object)->decltype(&std::remove_cvref_t<decltype(object)>::amount){return &std::remove_cvref_t<decltype(object)>::amount;};
    auto initial=[=]{return Methods::inferred_named_member(factory,"amount",field,call,pointer,inferred::Marker{});};
    auto formula=[=](ReadContext& context){return Methods::inferred_named_member(context,factory,"amount",field,call,pointer,[](ReadContext&){},inferred::Marker{});};
    auto current=inferred::make<inferred::Mode::readonly>(source,graph,initial,formula);auto fixed=inferred::make<inferred::Mode::fixed>(source,graph,initial,formula);auto variable=inferred::make<inferred::Mode::variable>(source,graph,initial,formula);
    source->set(V(V::Object{{"amount",V(4)}}));check(current->get()==V(4)&&fixed->get()==V(2)&&variable->get()==V(2),"Inferred map data treated as a method or storage modes changed");
    source->set(V(V::Object{}));check(current->get()==V(4)&&!graph->failures()->empty(),"Missing inferred map key lost last valid value");source->set(V(V::Object{{"amount",V(8)}}));check(current->get()==V(8)&&graph->failures()->empty(),"Inferred map data did not recover");
    check(bool(graph->close()),"Map graph close failed");
}

int main(int argc,char** argv){try{const std::string name=argc>1?argv[1]:"scalar";if(name=="scalar")scalar();else if(name=="methods")methods();else if(name=="factories")factories();else if(name=="aliases")aliases();else if(name=="observation")observation();else if(name=="reentry")reentry();else if(name=="maps")maps();else throw std::runtime_error("Unknown inferred native test");std::cout<<"Native inferred value "<<name<<" passed\n";}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
