#pragma once
#include <iiSDKCore/Core/Property/Observable.h>

struct NativeFocusRingModel final {
    ii::core::Observable<double> width{120};
    ii::core::Observable<bool> active{false};
};
