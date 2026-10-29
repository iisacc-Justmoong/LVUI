#pragma once
#include <LVUI/ListItem.h>
#include <LVUI/ListItemSelector.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedListItemSelectModel final {
    ii::core::Observable<std::vector<lvui::ListItemOption>> options{{{"Draft",true},{"Unavailable",false},{"Published",true}}};
    ii::core::Observable<int> index{0};
    ii::core::Observable<bool> enabled{true};
    std::vector<int> changes;
    std::vector<std::string> edits;
    void changed(int value){changes.push_back(value);index.set(value);}
    void edited(std::string field,const lvui::ListItemValue& value){if(std::holds_alternative<std::int64_t>(value))edits.push_back(std::move(field));}
};
