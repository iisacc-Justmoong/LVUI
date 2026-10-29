#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <vector>

struct NativeOwnedSliderModel final {
    ii::core::Observable<double> value{5},maximum{10},width{100};
    ii::core::Observable<bool> enabled{true};
    std::vector<double> values;
    void moved(double next){values.push_back(next);value.set(next);}
};
