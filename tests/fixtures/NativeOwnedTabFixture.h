#pragma once
#include <LVUI/Tab.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
struct NativeOwnedTabModel final {
    ii::core::Observable<std::string> text{std::string("Overview")},name{std::string("home")},badge{std::string("3")},
        accessibility{std::string("Overview tab")};
    ii::core::Observable<bool> enabled{true},selected{true};
    ii::core::Observable<double> size{18},width{80};
    ii::core::Observable<lvui::TabStyle> style{lvui::TabStyle::underline};
    int clicks=0;
    void clicked(){++clicks;}
};
