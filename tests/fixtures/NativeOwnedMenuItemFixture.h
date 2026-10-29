#pragma once
#include <LVUI/MenuItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
struct NativeOwnedMenuItemModel final {
    ii::core::Observable<std::string> label{std::string("Open")},shortcut{std::string("⌘O")},icon{std::string("procedure")};
    ii::core::Observable<lvui::MenuEntryState> state{lvui::MenuEntryState::normal};
    ii::core::Observable<lvui::MenuDirection> direction{lvui::MenuDirection::right};
    ii::core::Observable<bool> enabled{true},visible{true};
    int clicks=0,presses=0,releases=0;
    void clicked(){++clicks;}void pressed(){++presses;}void released(){++releases;}
};
