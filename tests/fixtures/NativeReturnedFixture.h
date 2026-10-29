#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
struct NativeReturnedBackend final {
    ii::core::Observable<int> amount{1};
    ii::core::Observable<bool> visible{true};
    int factories=0,total=0,text_factories=0;
    std::string text;
    static inline std::function<void()> during;
    auto make(double value){
        ++factories;
        return [this,amount=std::make_unique<int>(int(value))]{
            if(auto callback=std::exchange(during,{}))callback();
            total+=*amount;
        };
    }
    auto makeText(){++text_factories;return [this](std::string value){text=std::move(value);};}
};
