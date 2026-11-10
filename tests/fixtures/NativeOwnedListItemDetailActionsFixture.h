#pragma once
#include <LVUI/ListFooter.h>
#include <LVUI/ListItemSelector.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedListItemDetailActionsModel final {
    ii::core::Observable<std::string> metadata{"Illustration"};
    ii::core::Observable<bool> show_metadata{true};
    ii::core::Observable<std::vector<std::string>> options{{"Rename","Delete"}};
    std::vector<std::string> events;
    void action(std::string action,const lvui::ListFooterSlot&){events.push_back(action);}
    void menu(int index,const lvui::ListItemOption& item){events.push_back(std::to_string(index)+":"+item.text);}
};
