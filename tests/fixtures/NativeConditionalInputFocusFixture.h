#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <utility>

struct NativeConditionalFocusBackend final {
    ii::core::Observable<bool> show{false}, inner{false}, enabled{true}, visible{true};
    int submissions = 0;
    bool retire_on_submit = false;
    std::function<void()> during_submit;
    void submit() {
        ++submissions;
        if (during_submit) std::exchange(during_submit,{})();
        if (retire_on_submit) {
            retire_on_submit = false;
            show.set(false);
        }
    }
};
