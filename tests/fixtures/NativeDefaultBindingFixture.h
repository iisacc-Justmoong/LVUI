#pragma once

#include <iiSDKCore/Core/Property/Observable.h>

#include <cstdint>
#include <string>
#include <vector>

struct NativeDefaultBindingModel final {
    ii::core::Observable<std::string> text{"Draft"};
    ii::core::Observable<std::string> guarded{"Guarded"};
    ii::core::Observable<std::string> editor{"Editor"};
    ii::core::Observable<std::string> query{"Find"};
    const ii::core::Observable<std::string> locked{"Locked"};
    ii::core::Observable<bool> checked{false};
    ii::core::Observable<bool> guardedCheck{false};
    ii::core::Observable<bool> enabled{false};
    ii::core::Observable<bool> radio{false};
    ii::core::Observable<double> level{0.2};
    ii::core::Observable<std::int64_t> tab{0}, choice{0};
    std::vector<std::string> edits;
    std::vector<bool> toggles;

    void edited(const std::string& value) {
        edits.push_back(value);
    }
    void toggled(bool value) {
        toggles.push_back(value);
    }
};
