#pragma once

#include <iiSDKCore/Core/Property/Observable.h>
#include <cstdint>
#include <limits>

class BrowserDemoModel final {
public:
    ii::core::Observable<std::int64_t> count{0};

    void increment() {
        const auto current = count.get();
        if (current < std::numeric_limits<std::int64_t>::max()) count.set(current + 1);
    }

    void reset() { count.set(0); }
};
