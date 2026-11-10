#pragma once
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedListItemTaskModel final {
    ii::core::Observable<bool> checked{false};
    ii::core::Observable<int> priority{0};
    ii::core::Observable<double> quantity{2};
    ii::core::Observable<lvui::ListItemVariant> kind{lvui::ListItemVariant::detail_settings};
    ii::core::Observable<std::vector<std::string>> priorities{{"Low","High"}};
    std::vector<std::string> events;
    void checkedChanged(bool value){checked.set(value);events.push_back(value?"checked:true":"checked:false");}
    void priorityChanged(int value){priority.set(value);events.push_back("priority:"+std::to_string(value));}
    void quantityChanged(double value){quantity.set(value);events.push_back("quantity:"+std::to_string(int(value)));}
};
