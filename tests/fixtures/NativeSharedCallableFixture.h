#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <memory>
#include <vector>
#include <utility>
#include <stdexcept>
struct NativeSharedCallableBackend final {
    static inline int constructions=0,destructions=0;
    static inline std::function<void()> during;
    static inline std::vector<std::weak_ptr<int>> resources;
    ii::core::Observable<int> amount{1};
    ii::core::Observable<bool> visible{true},child_visible{true};
    int factories=0;std::vector<int> seen;
    NativeSharedCallableBackend(){++constructions;}
    ~NativeSharedCallableBackend(){++destructions;}
    struct Handler final {
        NativeSharedCallableBackend* owner;std::unique_ptr<int> state;std::shared_ptr<int> resource;
        void operator()(){const auto value=++*state;if(auto function=std::exchange(during,{}))function();owner->seen.push_back(value);}
    };
    struct Resident final {
        NativeSharedCallableBackend* owner;int calls=0;
        explicit Resident(NativeSharedCallableBackend* owner):owner(owner){}
        Resident(const Resident&)=delete;Resident(Resident&&)=delete;
        void operator()(){owner->seen.push_back(++calls);}
    } resident{this};
    Resident& borrow(){++factories;return resident;}
    auto make(double amount){
        ++factories;if(amount<0)throw std::runtime_error("Shared component factory failed");
        auto resource=std::make_shared<int>(0);resources.push_back(resource);
        return Handler{this,std::make_unique<int>(int(amount)),std::move(resource)};
    }
};
struct NativeSharedCallableShadow final {};
