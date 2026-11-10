#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>
struct NativeOwnedHelpButtonModel final {
    ii::core::Observable<std::string> text{std::string("Info")};
    ii::core::Observable<double> width{32};
    ii::core::Observable<bool> enabled{true},motion_enabled{true},enter_enabled{false};
    int clicks=0;
    std::vector<std::string> events;
    void open(){++clicks;events.emplace_back("clicked");}
    void press(){events.emplace_back("pressed");}
    void release(){events.emplace_back("released");}
    void cancel(){events.emplace_back("canceled");}
    void method(){events.emplace_back("method");}
    void after(){events.emplace_back("after");}
    void last(){events.emplace_back("last");}
};
