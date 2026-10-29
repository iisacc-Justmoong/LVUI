#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedTabBarSlotsModel final {
    ii::core::Observable<int> index{0};
    ii::core::Observable<std::string> title{"Second"};
    std::vector<int> selections;
    int clicks = 0;
    void changed(int value) { selections.push_back(value); index.set(value); }
    void clicked() { ++clicks; }
};
