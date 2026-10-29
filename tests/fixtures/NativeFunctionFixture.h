#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
struct NativeFunctionBackend final {
    static inline std::function<void()> during;
    ii::core::Observable<int> count{2},amount{1};
    ii::core::Observable<bool> visible{true};
    ii::core::Observable<int> file_revision{0};
    int factories=0;
    std::string format(std::int64_t value){if(value<0)throw std::runtime_error("Negative formatter input");return "D"+std::to_string(value);}
    bool accepts(std::int64_t value)const{return value>0;}
    auto makeFormatter(std::int64_t value){
        ++factories;if(value<0)throw std::runtime_error("Invalid formatter configuration");
        return [state=std::make_unique<std::int64_t>(value)](std::int64_t input){
            if(auto function=std::exchange(during,{}))function();
            if(input<0)throw std::runtime_error("Negative formatter input");return std::to_string(*state+input);
        };
    }
};
