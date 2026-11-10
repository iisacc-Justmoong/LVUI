#pragma once
#include <iiGUI/Image.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>
#include <vector>
struct NativeOwnedDropdownButtonModel final {
    ii::core::Observable<std::string> text{std::string("Open")};
    ii::core::Observable<double> width{100};
    ii::core::Observable<bool> enabled{true},iconMode{false},force{true};
    ii::core::Observable<std::shared_ptr<const ii::gui::RasterImage>> icon{};
    int clicks=0,presses=0,releases=0;
    std::vector<std::string> events;
    void clicked(){++clicks;events.emplace_back("clicked");}
    void pressed(){++presses;events.emplace_back("pressed");}
    void released(){++releases;events.emplace_back("released");}
    void method(){events.emplace_back("method");}
    void after(){events.emplace_back("after");}
    void last(){events.emplace_back("last");}
};
