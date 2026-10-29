#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
struct NativeOwnedHelpButtonModel final {
    ii::core::Observable<std::string> text{std::string("Info")};
    ii::core::Observable<double> width{32};
    ii::core::Observable<bool> enabled{true};
    int clicks=0;
    void open(){++clicks;}
};
