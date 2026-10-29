#pragma once
#include <LVUI/AppCard.h>
#include <iiGUI/Appearance.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>

struct NativeOwnedAppCardModel final {
    ii::core::Observable<std::string> title{std::string("Title")},subtitle{std::string("Detail")};
    ii::core::Observable<double> width{320.0};
    ii::core::Observable<lvui::AppCardShape> shape{lvui::AppCardShape::round_rect};
    ii::core::Observable<ii::gui::WidgetColor> background{ii::gui::WidgetColor{.2,.3,.4,1}};
};
