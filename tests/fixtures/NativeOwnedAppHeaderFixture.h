#pragma once
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeOwnedAppHeaderModel final {
    ii::core::Observable<std::string> title{std::string("Project")},subtitle{std::string("Editor")};
    ii::core::Observable<bool> compact{false},menu_visible{true};
    ii::core::Observable<std::string> accent{std::string("blue")};
    int menu_count=0;
    void menu(){++menu_count;}
};
