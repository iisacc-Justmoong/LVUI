#pragma once
#include <iiSDKCore/Core/Property/Observable.h>

struct NativeEmbeddedBranchBackend final {
    ii::core::Observable<bool> shown{false};
};
