#pragma once
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeOwnedListItemCustomTrailingModel final {
    ii::core::Observable<lvui::ListItemVariant> kind{lvui::ListItemVariant::toggle};
    ii::core::Observable<double> width{64},height{20};
    ii::core::Observable<std::string> label{"Custom"};
    unsigned trailing_clicks=0,leading_clicks=0,footer_clicks=0;
    void trailingClicked(){++trailing_clicks;}
    void leadingClicked(){++leading_clicks;}
    void footerClicked(){++footer_clicks;}
};
