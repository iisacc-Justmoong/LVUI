#pragma once

#include <iiSDKCore/Core/Property/Observable.h>
#include <iiGUI/Appearance.h>
#include <string>

struct NativeWindowAccentModel final {
    ii::core::Observable<std::string> accent{std::string("blue")};
    ii::core::Observable<std::string> label{std::string("Nested")};
    ii::core::Observable<double> width{640};
    ii::core::Observable<double> customWidth{140};
    ii::core::Observable<ii::gui::WidgetColor> customColor{ii::gui::WidgetColor{.1,.8,.2,1}};
    int clicks = 0;
    void clicked() { ++clicks; }
};
