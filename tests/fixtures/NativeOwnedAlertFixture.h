#pragma once
#include <LVUI/NativeAlert.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <stdexcept>
#include <string>
#include <vector>

struct NativeOwnedAlertModel final {
    ii::core::Observable<std::string> title{"Initial"}, message{"Details"};
    ii::core::Observable<int> count{2};
    ii::core::Observable<bool> secondaryEnabled{true};
    std::vector<std::string> events;
    void apply(const lvui::native::AlertButtonEvent& event) {
        if (event.trigger != "clicked" || !event.source.snapshot()) throw std::runtime_error("Invalid generated alert event");
        events.emplace_back("method");
    }
    void clicked() { events.emplace_back("clicked"); }
    void dismissed() { events.emplace_back("dismissed"); }
};
