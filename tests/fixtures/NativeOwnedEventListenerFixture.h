#pragma once
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <utility>
#include <vector>

struct NativeOwnedEventListenerModel final {
    ii::core::Observable<double> width{320.0},height{180.0},magnification{1.0};
    std::vector<ii::gui::WidgetInput> events;
    std::vector<ii::gui::Rect> layouts;
    std::vector<std::pair<double,double>> sizes;
    std::vector<std::string> dropped;
    bool accepts_drop=true,accepts_pinch=true;
    int pinch_count=0;
    int bool_count=0, hover_count=0, focus_count=0;
    ii::core::Result<bool> input(const ii::gui::WidgetInput& value){
        events.push_back(value);
        return true;
    }
    bool pinch(double factor){++pinch_count;if(!accepts_pinch)return false;magnification.set(magnification.get()*factor);return true;}
    bool input_bool(const ii::gui::WidgetInput&){++bool_count;return true;}
    bool drop(const std::vector<std::string>& files){dropped=files;return accepts_drop;}
    void resized(ii::gui::Rect value){layouts.push_back(value);}
    void sizeChanged(double width,double height){sizes.emplace_back(width,height);}
    void hovered(){++hover_count;}
    void focused(){++focus_count;}
};
