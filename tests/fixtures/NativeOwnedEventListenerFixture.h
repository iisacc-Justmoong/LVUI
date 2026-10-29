#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <vector>

struct NativeOwnedEventListenerModel final {
    ii::core::Observable<double> width{320.0},height{180.0};
    std::vector<ii::gui::WidgetInput> events;
    std::vector<ii::gui::Rect> layouts;
    int bool_count=0;
    ii::core::Result<bool> input(const ii::gui::WidgetInput& value){events.push_back(value);return true;}
    bool input_bool(const ii::gui::WidgetInput&){++bool_count;return true;}
    void resized(ii::gui::Rect value){layouts.push_back(value);}
};
