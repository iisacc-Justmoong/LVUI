#pragma once
#include <LVUI/Motion.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedToggleSwitchModel final {
    ii::core::Observable<std::string> text{std::string("Snap")};
    ii::core::Observable<bool> checked{false},enabled{true},rtl{false},motion_enabled{false};
    ii::core::Observable<double> width{38};
    ii::core::Observable<ii::gui::WidgetColor> on_color{ii::gui::WidgetColor{.0812049285,.5099760294,1,1}};
    ii::core::Observable<lvui::Motion> motion{lvui::Motion{.duration=std::chrono::milliseconds{320},.curve=lvui::MotionCurve::linear}};
    std::vector<bool> events;
    void changed(bool value){events.push_back(value);}
};
