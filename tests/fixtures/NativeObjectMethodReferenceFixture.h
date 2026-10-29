#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <array>
#include <memory>
#include <string>
#include <vector>
namespace DeferredDomain {
class Item final:public std::enable_shared_from_this<Item> {
    ii::core::Observable<int> count_{0};
public:
    static inline int live=0;
    const std::string name;std::string last;std::function<void()> during;int factories=0;
    explicit Item(std::string title):name(std::move(title)){++live;}~Item(){--live;}
    Item(const Item&)=delete;Item(Item&&)=delete;
    void act()&{count_.set(count_.get()+1);if(auto action=std::exchange(during,{}))action();(void)count_.get();}
    void record(std::string value){last=std::move(value);}
    int calls()const{return count_.get();}
    std::string describe()const{return name+" "+std::to_string(count_.get());}
    auto makeHandler(){++factories;return [self=shared_from_this(),token=std::make_unique<int>(1)]{self->act();};}
};
struct Parent final {Item resident{"Parent"};};
struct Temporary final {
    static inline int live=0;int* calls;
    explicit Temporary(int* value):calls(value){++live;}~Temporary(){--live;}
    Temporary(const Temporary&)=delete;Temporary(Temporary&&)=delete;
    void act()&&{++*calls;}
};
struct OwnedParent final {
    static inline int live=0;
    struct Child final {int* calls;void act()const{++*calls;}} child;
    explicit OwnedParent(int* calls):child{calls}{++live;}~OwnedParent(){--live;}
    OwnedParent(const OwnedParent&)=delete;OwnedParent(OwnedParent&&)=delete;
    const Child& selected()const{return child;}
};
struct Indexed final {int calls=0;Indexed()=default;Indexed(const Indexed&)=delete;Indexed(Indexed&&)=delete;void act(){++calls;}};
}
struct NativeObjectMethodReferenceBackend final {
    static inline int live=0,created=0;
    std::shared_ptr<DeferredDomain::Item> first=std::make_shared<DeferredDomain::Item>("First"),second=std::make_shared<DeferredDomain::Item>("Second");
    ii::core::Observable<std::shared_ptr<DeferredDomain::Item>> selected{first};
    ii::core::Observable<std::vector<std::shared_ptr<DeferredDomain::Item>>> rows{{first}};
    ii::core::Observable<std::shared_ptr<DeferredDomain::Parent>> parent{std::make_shared<DeferredDomain::Parent>()};
    ii::core::Observable<int> file_revision{0};ii::core::Observable<bool> visible{true};
    mutable int getters=0;int temporary_calls=0,owned_parent_calls=0;
    std::array<DeferredDomain::Indexed,1> indexed;
    NativeObjectMethodReferenceBackend(){++live;++created;}~NativeObjectMethodReferenceBackend(){--live;}
    std::shared_ptr<DeferredDomain::Item> current()const{++getters;return selected.get();}
    DeferredDomain::Temporary temporary(){return DeferredDomain::Temporary{&temporary_calls};}
    DeferredDomain::OwnedParent ownedParent(){return DeferredDomain::OwnedParent{&owned_parent_calls};}
};
