#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>
struct NativeOwnedMenuModel final {
    ii::core::Observable<int> index{-1};
    ii::core::Observable<double> width{150},thickness{1},opacity{.12};
    ii::core::Observable<std::string> label{"First"};
    ii::core::Observable<bool> keepOpen{true},secondEnabled{false},dividerVisible{true};
    std::vector<int> selections;int clicks=0,opens=0,closes=0;
    void triggered(int value){selections.push_back(value);}
    void clicked(){++clicks;}
    void opened(){++opens;}
    void closed(){++closes;}
};
