#pragma once
#include <LVUI/Material.h>
#include <iiGUI/Appearance.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
struct NativeOwnedMaterialModel final {
    ii::core::Observable<double> width{120},height{80},blur{16};
    ii::core::Observable<ii::gui::WidgetColor> accent{ii::gui::WidgetColor{1,0,0,1}};
    ii::core::Observable<lvui::MaterialDensity> density{lvui::MaterialDensity::glass25};
    ii::core::Observable<lvui::MaterialBackdrop> backdrop{lvui::MaterialBackdrop::previous_layers};
    ii::core::Observable<bool> enabled{true};
    ii::core::Observable<std::string> outline{std::string{}};
    ii::core::Observable<lvui::Motion> motion{lvui::Motion{}};
};
