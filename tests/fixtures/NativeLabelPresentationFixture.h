#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
struct NativeLabelPresentationModel final {
    ii::core::Observable<std::string> text{"A long description used by the application to describe an item and its current state"};
    ii::core::Observable<bool> elide{false};
    ii::core::Observable<int> alignment{0};
};
