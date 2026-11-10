#pragma once
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeOwnedListItemCustomLeadingModel final {
    ii::core::Observable<lvui::ListItemVariant> kind{lvui::ListItemVariant::navigation};
    ii::core::Observable<bool> shown{true};
    ii::core::Observable<double> size{14};
    ii::core::Observable<std::string> label{"L"};
    unsigned leading_clicks=0,footer_clicks=0;
    void leadingClicked(){++leading_clicks;}
    void footerClicked(){++footer_clicks;}
};
