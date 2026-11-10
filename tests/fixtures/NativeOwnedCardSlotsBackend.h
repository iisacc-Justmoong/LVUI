#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
struct NativeOwnedCardSlotsBackend final {
    ii::core::Observable<std::string> caption{"Initial"};
    unsigned clicks=0;
    void clicked(){++clicks;}
};
