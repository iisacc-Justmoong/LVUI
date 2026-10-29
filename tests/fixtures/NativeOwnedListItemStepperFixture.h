#pragma once
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedListItemStepperModel final {
    ii::core::Observable<double> quantity{2};
    ii::core::Observable<bool> enabled{true};
    std::vector<double> changes;
    std::vector<std::string> edits;
    void changed(double value){changes.push_back(value);quantity.set(value);}
    void edited(std::string field,const lvui::ListItemValue& value){if(std::holds_alternative<double>(value))edits.push_back(std::move(field));}
};
