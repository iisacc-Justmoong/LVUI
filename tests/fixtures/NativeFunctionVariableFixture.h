#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <memory>
#include <string>
#include <utility>
struct NativeFunctionVariableBackend final {
    static inline std::function<void()> during;
    ii::core::Observable<int> count{2},amount{1},file_revision{0};
    ii::core::Observable<bool> visible{true};
    int factories=0,events=0;
    std::string format(std::int64_t n)const{return "D"+std::to_string(n);}
    void increment(){++events;}
    auto makeFormatter(std::int64_t n){++factories;return [state=std::make_unique<std::int64_t>(n)](std::int64_t input){
        if(auto fn=std::exchange(during,{}))fn();return std::to_string(*state+input);
    };}
};
