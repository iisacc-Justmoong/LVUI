#pragma once
#include <LVUI/NativeAlertButton.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <stdexcept>
#include <string>
#include <vector>

struct NativeOwnedAlertButtonModel final {
    ii::core::Observable<std::string> text{"Cancel"};
    ii::core::Observable<lvui::ButtonTone> tone{lvui::ButtonTone::normal};
    ii::core::Observable<bool> dialog{true}, motion_enabled{true}, enter_enabled{false};
    ii::core::Observable<double> height{56};
    std::vector<std::string> events;
    std::string source_id;
    void method(const lvui::native::ButtonEventValue<ii::gui::Widget>& event) {
        auto snapshot = event.source.snapshot();
        if (!snapshot || event.trigger != "clicked") throw std::runtime_error("Invalid alert method event");
        source_id = snapshot->id; events.push_back("method");
    }
    void clicked() { events.push_back("clicked"); }
    void after() { events.push_back("after"); }
    void last(const lvui::native::ButtonEventValue<ii::gui::Widget>&) { events.push_back("last"); }
    void pressed() { events.push_back("pressed"); }
    void released() { events.push_back("released"); }
    void canceled() { events.push_back("canceled"); }
};
