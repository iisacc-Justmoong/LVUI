#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeAbstractInputBarModel final {
    ii::core::Observable<std::string> text{"Draft"};
};
