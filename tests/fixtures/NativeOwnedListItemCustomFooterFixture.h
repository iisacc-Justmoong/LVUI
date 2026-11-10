#pragma once
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeOwnedListItemCustomFooterModel final {
    ii::core::Observable<lvui::ListItemVariant> kind{lvui::ListItemVariant::detail_actions};
    ii::core::Observable<double> height{30};
    ii::core::Observable<std::string> label{"Custom"};
    unsigned clicks=0;
    void clicked(){++clicks;}
};
