#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedInputFieldModel final {
    ii::core::Observable<std::string> text{"Draft"},placeholder{"Enter"},clearName{"Clear draft"};
    ii::core::Observable<double> width{160},maxLength{64};
    ii::core::Observable<int> size{0},surface{0};
    ii::core::Observable<bool> enabled{true},readOnly{false},password{false},wrap{true},clear{false};
    std::vector<std::string> edits;
    unsigned submissions=0;
    void edited(std::string value){edits.push_back(value);text.set(std::move(value));}
    void submit(){++submissions;}
};
