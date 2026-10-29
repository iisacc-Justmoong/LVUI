#pragma once
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeOwnedListItemModel final {
    ii::core::Observable<std::string> label{"Camera"}, description{"Shared"};
    ii::core::Observable<bool> enabled{true}, selected{false}, visible{true};
    ii::core::Observable<double> width{280};
    int clicks = 0;
    void clicked() { ++clicks; }
};
