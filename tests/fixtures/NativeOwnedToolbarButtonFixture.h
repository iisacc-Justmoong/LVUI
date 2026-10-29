#pragma once
#include <LVUI/ToolbarButton.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <cstdint>
#include <string>
#include <vector>

struct NativeOwnedToolbarButtonModel final {
    ii::core::Observable<lvui::ToolbarButtonId> identity{lvui::ToolbarButtonId{std::string("open")}};
    ii::core::Observable<std::string> icon{"home"}, text{"Open"};
    ii::core::Observable<bool> visible{true}, enabled{true};
    ii::core::Observable<double> width{100};
    int clicks = 0; std::vector<lvui::ToolbarButtonId> ids;
    void clicked() { ++clicks; }
    void triggered(const lvui::ToolbarButtonId& value) { ids.push_back(value); }
};
