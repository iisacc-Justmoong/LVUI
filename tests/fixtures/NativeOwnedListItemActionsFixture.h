#pragma once
#include <LVUI/ListFooter.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedListItemActionsModel final {
    ii::core::Observable<std::string> label{"Project"};
    ii::core::Observable<lvui::ListFooterSlot> primary{lvui::ListFooterSlot{.text="Open"}};
    ii::core::Observable<bool> show_secondary{true};
    std::vector<std::string> actions;
    void triggered(std::string action,const lvui::ListFooterSlot& slot){actions.push_back(action+":"+slot.text);}
};
