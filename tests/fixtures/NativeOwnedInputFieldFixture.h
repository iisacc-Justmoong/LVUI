#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedInputFieldModel final {
    ii::core::Observable<std::string> text{"Draft"},placeholder{"Enter"};
    ii::core::Observable<double> width{160};
    ii::core::Observable<bool> enabled{true},readOnly{false},password{false};
    std::vector<std::string> edits;
    unsigned submissions=0;
    void edited(std::string value){edits.push_back(value);text.set(std::move(value));}
    void submit(){++submissions;}
};
