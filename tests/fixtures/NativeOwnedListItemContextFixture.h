#pragma once
#include <LVUI/List.h>
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <any>
#include <string>
#include <utility>
#include <vector>

struct NativeOwnedListItemContextEntry final { std::string key; friend bool operator==(const NativeOwnedListItemContextEntry&,const NativeOwnedListItemContextEntry&)=default; };
struct NativeOwnedListItemContextModel final {
    ii::core::Observable<std::string> label{"Alpha"};
    ii::core::Observable<bool> checked{false};
    ii::core::Observable<double> quantity{1};
    ii::core::Observable<std::string> payload_value{"asset-42"};
    ii::core::Observable<std::vector<NativeOwnedListItemContextEntry>> entries{{{"first"},{"second"}}};
    std::shared_ptr<lvui::ListMemory> source=lvui::ListMemory::create({{.key="a",.label="Row A"},{.key="b",.label="Row B"}}).value();
    std::vector<std::pair<std::string,lvui::ListItemValue>> edits;
    unsigned actions=0;
    unsigned payload_actions=0;
    std::vector<std::string> payload_values;
    void edited(std::string name,const lvui::ListItemValue& value){
        edits.emplace_back(name,value);
        if(name=="checked")checked.set(std::get<bool>(value));
        if(name=="quantity")quantity.set(std::get<double>(value));
    }
    void actionEvent(std::string action,const lvui::ListItemActionPayload& payload){
        if(action=="primary")if(const auto* values=std::get_if<lvui::ListItemActionValues>(&payload)){
            if(!values->custom_payload)++actions;
            else if(values->checked){++payload_actions;payload_values.push_back(std::any_cast<std::string>(*values->custom_payload));}
        }
    }
};
