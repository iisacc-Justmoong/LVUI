#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <vector>
struct NativeOwnedTabBarModel final {
    ii::core::Observable<int> index{0};
    ii::core::Observable<bool> automatic{true},middleEnabled{false},lastVisible{true};
    ii::core::Observable<double> spacing{4},minimum{120};
    std::vector<int> selections;int clicks=0;
    void changed(int value){selections.push_back(value);index.set(value);}
    void clicked(){++clicks;}
};
