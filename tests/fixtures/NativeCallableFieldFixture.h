#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <memory>
#include <string>
namespace CallableFieldDomain {
struct Handler final {
    static inline int live=0;
    std::unique_ptr<int> count=std::make_unique<int>(0);
    ii::core::Observable<int> completed{0};std::function<void()> during;
    Handler(){++live;}~Handler(){--live;}Handler(const Handler&)=delete;Handler(Handler&&)=delete;
    void operator()(){const auto next=++*count;if(auto action=std::exchange(during,{}))action();completed.set(next);}
};
struct Node final {ii::core::Observable<std::shared_ptr<Handler>> run{std::make_shared<Handler>()};};
struct Borrowed final {
    ii::core::Observable<int> count{0};Borrowed()=default;Borrowed(const Borrowed&)=delete;
    void operator()()&{count.set(count.get()+1);}
};
struct Parent final {
    static inline int live=0;
    struct Action final {int* count;void operator()()const{++*count;}} run;
    explicit Parent(int* count):run{count}{++live;}~Parent(){--live;}
    Parent(const Parent&)=delete;Parent(Parent&&)=delete;
};
}
struct NativeCallableFieldBackend final {
    struct Readable final {
        std::function<void()> value;std::uint64_t version=0;
        const auto& get()const{return value;}std::uint64_t revision()const{return version;}
        void set(std::function<void()> next){value=std::move(next);++version;}
    };
    static inline int live=0,created=0;
    ii::core::Observable<std::shared_ptr<CallableFieldDomain::Handler>> run{std::make_shared<CallableFieldDomain::Handler>()};
    ii::core::Observable<std::function<std::string(std::int64_t)>> format{[](auto value){return "First "+std::to_string(value);}};
    ii::core::Observable<std::function<void(std::string)>> edited{[this](auto value){last=std::move(value);}};
    ii::core::Observable<std::shared_ptr<CallableFieldDomain::Node>> selected{std::make_shared<CallableFieldDomain::Node>()};
    ii::core::Observable<bool> visible{true};ii::core::Observable<int> file_revision{0};
    CallableFieldDomain::Borrowed borrowed;
    Readable readable{[this]{last="First";}};
    int temporary_calls=0;std::string last;
    NativeCallableFieldBackend(){++live;++created;}~NativeCallableFieldBackend(){--live;}
    CallableFieldDomain::Parent temporary(){return CallableFieldDomain::Parent{&temporary_calls};}
    void changeReadable(){readable.set([this]{last="Second";});}
};
