#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <cstdint>
#include <string>

struct NativeOwnedHierarchyItemModel final {
    ii::core::Observable<std::string> label{"Camera"};
    ii::core::Observable<std::int64_t> count{12};
    ii::core::Observable<bool> expanded{false}, children{true};
    int activations = 0, expansions = 0;
    void clicked() { ++activations; }
    void changed(bool value) { ++expansions; expanded.set(value); }
};
