#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeMobileTabsModel final {
    ii::core::Observable<std::string> label{"Home"},badge{"3"};
    ii::core::Observable<bool> selected{true};
    int clicks=0;
    void activate(){++clicks;}
};
