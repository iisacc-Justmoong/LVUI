#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
struct NativeCallableBackend final {
    static inline int constructions=0,destructions=0;
    static inline std::function<void()> during;
    static inline std::weak_ptr<int> latest_resource;
    static inline NativeCallableBackend* last_text_receiver=nullptr;
    ii::core::Observable<int> amount{1};
    ii::core::Observable<bool> visible{true};
    int calls=0,factories=0;
    std::string text;
    NativeCallableBackend(){++constructions;}
    ~NativeCallableBackend(){++destructions;}
    void save(){if(auto function=std::exchange(during,{}))function();++calls;}
    void record(std::string input){last_text_receiver=this;text=std::move(input);}
    auto make(double input){
        ++factories;auto resource=std::make_shared<int>(int(input));latest_resource=resource;
        return [this,value=std::make_unique<int>(int(input)),resource=std::move(resource)]{
            (void)resource;
            if(auto function=std::exchange(during,{}))function();calls+=*value;
        };
    }
    auto makeText(){return [this](std::string input){record(std::move(input));};}
};
struct NativeCallableShadow final {};
