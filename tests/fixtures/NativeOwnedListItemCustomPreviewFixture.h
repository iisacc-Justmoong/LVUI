#pragma once
#include <LVUI/ListItem.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeOwnedListItemCustomPreviewModel final {
    ii::core::Observable<lvui::ListItemVariant> kind{lvui::ListItemVariant::resource};
    ii::core::Observable<double> size{48};
    ii::core::Observable<bool> shown{true};
    ii::core::Observable<std::string> label{"Preview"};
    unsigned previews=0,footers=0;
    void previewClicked(){++previews;}
    void footerClicked(){++footers;}
};
