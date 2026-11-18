#pragma once

#include <iiSDKCore/Core/Property/Observable.h>
#include <cstdint>
#include <limits>

class WebCounterModel final {
public:
    ii::core::Observable<std::int64_t> count{0};

    void increment() {
        const auto value = count.get();
        if (value < std::numeric_limits<std::int64_t>::max()) count.set(value + 1);
    }

    void reset() { count.set(0); }
};
