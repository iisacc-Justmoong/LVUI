#pragma once
#include <LVRS/Theme.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
struct NativeWindowAccentModel final {
    ii::core::Observable<lvrs::Accent> accent{lvrs::Accent::blue};
    ii::core::Observable<std::string> name{std::string("green")};
};
