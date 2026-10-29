#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <string>
struct NativeInferredFieldsBackend final {
    int count=0;
    ii::core::Observable<int> file_revision{0};
    std::function<void()> run=[this]{++count;};
    ii::core::Observable<std::function<std::string(std::int64_t)>> format{
        [](std::int64_t value){return "First "+std::to_string(value);}};
    void bump(){++count;}
    auto factory(){return [this]{++count;};}
};
