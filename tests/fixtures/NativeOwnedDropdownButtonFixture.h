#pragma once
#include <iiGUI/Image.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <memory>
#include <string>
struct NativeOwnedDropdownButtonModel final {
    ii::core::Observable<std::string> text{std::string("Open")};
    ii::core::Observable<double> width{100};
    ii::core::Observable<bool> enabled{true},iconMode{false},force{true};
    ii::core::Observable<std::shared_ptr<const ii::gui::RasterImage>> icon{};
    int clicks=0,presses=0,releases=0;
    void clicked(){++clicks;}
    void pressed(){++presses;}
    void released(){++releases;}
};
