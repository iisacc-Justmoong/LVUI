#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>
struct NativeOwnedCheckBoxModel final {
    ii::core::Observable<std::string> text{std::string("Remember")};
    ii::core::Observable<bool> checked{false},enabled{true};
    ii::core::Observable<double> size{17};
    ii::core::Observable<ii::gui::WidgetColor> color{ii::gui::WidgetColor{.0812049285,.5099760294,1,1}};
    std::vector<bool> events;
    void changed(bool value){events.push_back(value);}
};
