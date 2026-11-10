#pragma once
#include <LVUI/StyleTokens.h>
#include <iiSDKCore/Core/Property/Observable.h>
struct NativePanelColorsModel final {
    ii::core::Observable<lvui::StyleColor> color{lvui::StyleColor{.5, .25, .75, 1}};
    ii::core::Observable<double> width{2};
};
