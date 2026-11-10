#pragma once
#include <iiSDKCore/Core/Property/Observable.h>

struct NativeWindowNoiseModel final {
    ii::core::Observable<double> strength{0.3};
};
