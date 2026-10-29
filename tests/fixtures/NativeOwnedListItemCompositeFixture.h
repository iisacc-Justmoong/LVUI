#pragma once
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedListItemCompositeModel final {
    ii::core::Observable<lvui::ListItemVariant> kind{lvui::ListItemVariant::toggle};
    ii::core::Observable<std::string> label{"Camera"};
    ii::core::Observable<bool> checked{true}, enabled{true},visible{true};
    std::vector<bool> changes;std::vector<std::string> edits;
    void changed(bool value){changes.push_back(value);checked.set(value);}
    void edited(std::string field,const lvui::ListItemValue& value){if(std::holds_alternative<bool>(value))edits.push_back(std::move(field));}
};
