#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
struct NativeListItemValueModel final {
    ii::core::Observable<std::string> value{"512 GB"};
    ii::core::Observable<bool> show{true},trailing{true};
    unsigned activations=0;
    void activate(){++activations;}
};
