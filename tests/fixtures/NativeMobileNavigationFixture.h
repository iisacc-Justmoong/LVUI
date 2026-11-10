#pragma once
#include <LVUI/MobileNavigation.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <optional>
#include <vector>

struct NativeMobileNavigationModel final {
    ii::core::Observable<std::vector<lvui::MobileNavigationItem>> items{std::vector<lvui::MobileNavigationItem>{
        {"home","","home","Home"},{"tools","","collection-asset","Tools"},
        {"storage","","sqlFile","Storage"},{"notification","","toolwindownotifications","Notification"},
        {"account","","role","Account"}}};
    ii::core::Observable<std::optional<lvui::MobileNavigationItem>> searchItem{
        lvui::MobileNavigationItem{"search","","generalsearch","Search"}};
    ii::core::Observable<int> current{0};
    ii::core::Observable<int> platform{1};
    ii::core::Observable<bool> automatic{false};
    ii::core::Observable<double> width{402},inset{0};
    std::vector<int> activations;int searches=0;
    void activate(int value){activations.push_back(value);}
    void search(){++searches;}
};
struct NativeSocietyMobileNavigationModel final {
    ii::core::Observable<std::vector<lvui::MobileNavigationItem>> items{std::vector<lvui::MobileNavigationItem>{
        {"Dashboard","Home","home","Dashboard"},{"Tools","Tools","toolwindowbuild","Tools"},
        {"Storage","Storage","nodesfolder","Storage"},{"Browse","Browse","RemoteChanges","Browse"},
        {"Environment","Environment","generalsettings","Environment"}}};
    ii::core::Observable<int> current{0};ii::core::Observable<double> inset{0};
    std::vector<int> activations;
    void activate(int value){activations.push_back(value);}
};
