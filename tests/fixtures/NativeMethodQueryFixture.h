#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace MethodDomain {
struct Record final {std::string title;int count;std::string describe()&&{return title+" "+std::to_string(count);}};
class Item final {
    ii::core::Observable<int> amount_{3},other_{8};
    ii::core::Observable<bool> alternate_{false};
public:
    static inline int live=0;
    mutable int queries=0;mutable std::function<void()> on_query;
    const std::string name;
    explicit Item(std::string text):name(std::move(text)){++live;}~Item(){--live;}
    std::string format(std::int64_t suffix)const{
        ++queries;const int amount=alternate_.get()?other_.get():amount_.get();
        if(auto action=std::exchange(on_query,{}))action();
        if(amount<0)throw std::runtime_error("Invalid query amount");
        return name+" "+std::to_string(amount+suffix);
    }
    Record record()const{return {name,amount_.get()};}
    void set(int value){amount_.set(value);}void other(int value){other_.set(value);}void alternate(bool value){alternate_.set(value);}
};
}
struct NativeMethodQueryModel final {
    static inline int live=0;
    std::shared_ptr<MethodDomain::Item> first=std::make_shared<MethodDomain::Item>("First"),second=std::make_shared<MethodDomain::Item>("Second");
    ii::core::Observable<std::shared_ptr<const MethodDomain::Item>> selected{first};
    ii::core::Observable<std::vector<std::shared_ptr<const MethodDomain::Item>>> rows{{first,second}};
    ii::core::Observable<int> amount{2},file_revision{0};
    ii::core::Observable<bool> visible{true};
    NativeMethodQueryModel(){++live;}~NativeMethodQueryModel(){--live;}
    std::string format(std::int64_t suffix)const{return selected.get()->format(suffix);}
};
