#pragma once

#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeWindowAccentControlsModel final {
    ii::core::Observable<std::string> accent{std::string("blue")};
    ii::core::Observable<double> width{640};
    int edits = 0, toggles = 0, moves = 0, steps = 0, tabs = 0, opens = 0, closes = 0;
    void edited(std::string) { ++edits; }
    void toggled(bool) { ++toggles; }
    void moved(double) { ++moves; }
    void stepped(int) { ++steps; }
    void selected() { ++tabs; }
    void opened() { ++opens; }
    void closed() { ++closes; }
};
