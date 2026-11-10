#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
struct NativeOwnedScrollModel {
    ii::core::Observable<double> width{180}, height{80};
    ii::core::Observable<bool> enabled{true};
    int activated=0;
    void activate() { ++activated; }
};
