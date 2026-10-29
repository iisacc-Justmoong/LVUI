#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <iiColorManager/ColorValue.h>
#include <string>
#include <stdexcept>
#include <cstdint>
struct NativeScalarInputModel final {
    static inline int live = 0;
    ii::core::Observable<int> count{2};
    ii::core::Observable<bool> visible{true};
    ii::core::Observable<double> amount{.25}, size{12};
    ii::core::Observable<iiColorManager::Rgba> ink{iiColorManager::Rgba{.25,.5,.75,1}};
    ii::core::Observable<int> file_revision{0};
    int calls = 0;
    int formatter_factories = 0, formatter_calls = 0;
    bool fail = false;
    NativeScalarInputModel() { ++live; }
    ~NativeScalarInputModel() { --live; }
    std::string compute(double value) {
        ++calls;
        if (fail) throw std::runtime_error("Requested scalar input failure");
        return std::to_string(static_cast<int>(value)) + "#" + std::to_string(calls);
    }
    auto makeFormatter(double base) {
        ++formatter_factories;
        return [this, base](std::int64_t value) {
            return std::to_string(value) + ":" + std::to_string(static_cast<int>(base * 100)) + "#" + std::to_string(++formatter_calls);
        };
    }
};
