#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
struct NativeConditionalSpacingModel final {
    ii::core::Observable<bool> visible{true};
    ii::core::Observable<bool> primary{true};
    ii::core::Observable<double> height{20};
    ii::core::Observable<double> alternate_height{20};
    ii::core::Observable<int> file_revision{0};
};
