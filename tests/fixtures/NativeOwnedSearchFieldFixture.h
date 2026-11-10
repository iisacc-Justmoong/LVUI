#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedSearchFieldModel final {
    ii::core::Observable<std::string> query{"Draft"},placeholder{"Search"};
    ii::core::Observable<double> width{160};
    ii::core::Observable<int> surface{1};
    ii::core::Observable<bool> enabled{true},readOnly{false},clearVisible{true};
    std::vector<std::string> edits;
    unsigned submissions=0;
    void edited(std::string value){edits.push_back(value);query.set(std::move(value));}
    void submit(){++submissions;}
};
