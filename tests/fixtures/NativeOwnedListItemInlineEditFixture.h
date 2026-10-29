#pragma once
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedListItemInlineEditModel final {
    ii::core::Observable<std::string> text{"Name"};
    ii::core::Observable<bool> read_only{false};
    std::vector<std::string> changes,edits,submissions;
    void changed(std::string value){changes.push_back(value);text.set(std::move(value));}
    void submitted(std::string value){submissions.push_back(std::move(value));}
    void edited(std::string field,const lvui::ListItemValue& value){if(std::holds_alternative<std::string>(value))edits.push_back(std::move(field));}
};
