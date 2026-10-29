#pragma once

#include <iiSDKCore/Core/Property/Observable.h>
#include <cstdint>
#include <string>

struct NativeHierarchyCountViewModel final {
    ii::core::Observable<std::string> badge{std::string("Two")};
    ii::core::Observable<std::int64_t> count{2};
    ii::core::Observable<double> countWidth{40};
    ii::core::Observable<bool> enabled{true};
    int clicks = 0;
    void clicked() { ++clicks; }
};
