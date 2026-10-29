#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeOwnedDividerModel final {
    ii::core::Observable<std::string> axis{std::string("Horizontal")};
    ii::core::Observable<double> length{120},thickness{2},cross{2},inset{4};
    ii::core::Observable<ii::gui::WidgetColor> color{ii::gui::WidgetColor{1,0,0,.5}};
};
