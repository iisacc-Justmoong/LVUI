#pragma once
#include <algorithm>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeTextEditorModel final {
    ii::core::Observable<std::string> text{"Project"},placeholder{"Enter a goal"};
    ii::core::Observable<double> width{160},height{40},minimumHeight{72},inset{4};
    ii::core::Observable<bool> enabled{true},readOnly{false};
    std::vector<std::string> edits;
    std::vector<double> content_heights;
    bool adaptive=false;
    void edited(std::string value){edits.push_back(value);text.set(std::move(value));}
    void measured(double value){content_heights.push_back(value);if(adaptive)height.set(std::clamp(value+2*inset.get(),minimumHeight.get(),160.));}
};
