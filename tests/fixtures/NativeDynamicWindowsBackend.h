#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <functional>
#include <string>
#include <string_view>

struct NativeDynamicWindowTarget final {
    std::string value = "palette";
    std::function<void()> during_conversion;
    operator std::string_view() const {
        if (during_conversion) during_conversion();
        return value;
    }
};

struct NativeDynamicWindowsBackend final {
    ii::core::Observable<std::string> target{"tools"};
    ii::core::Observable<bool> show_nested{true};
    int queries = 0;
    std::function<void()> during_choose;
    NativeDynamicWindowTarget view_target;
    std::string choose() {
        ++queries;
        auto selected = target.get();
        if (during_choose) during_choose();
        return selected;
    }
};
