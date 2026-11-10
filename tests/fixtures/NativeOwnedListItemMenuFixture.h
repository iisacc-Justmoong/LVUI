#pragma once
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>

#include <any>
#include <memory>
#include <string>
#include <vector>

struct NativeOwnedListItemMenuModel final {
    static std::vector<lvui::ListItemMenuEntry> initial(){
        lvui::ListItemMenuEntry leaf;leaf.label="Export PNG";leaf.payload=std::make_shared<const std::any>(std::string("asset-42"));
        lvui::ListItemMenuEntry branch;branch.label="Export";branch.children={leaf};
        return {branch};
    }
    ii::core::Observable<std::vector<lvui::ListItemMenuEntry>> entries{initial()};
    std::vector<lvui::ListItemMenuSelection> selections;
    void selected(const lvui::ListItemMenuSelection& value){selections.push_back(value);}
};
