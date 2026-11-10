#pragma once
#include <LVUI/CheckBox.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>
struct NativeOwnedCheckBoxModel final {
    ii::core::Observable<std::string> text{std::string("Remember")};
    ii::core::Observable<bool> checked{false},enabled{true};
    ii::core::Observable<bool> useAsset{false};
    ii::core::Observable<bool> motionEnabled{false};
    ii::core::Observable<double> duration{360};
    ii::core::Observable<lvui::Motion> motion{lvui::Motion{}};
    ii::core::Observable<lvui::CheckBoxShape> shape{lvui::CheckBoxShape::round_rect};
    ii::core::Observable<double> size{17};
    ii::core::Observable<ii::gui::WidgetColor> color{ii::gui::WidgetColor{.0812049285,.5099760294,1,1}};
    ii::core::Observable<ii::gui::WidgetColor> softShadow{ii::gui::WidgetColor{0,0,0,20./255.}};
    ii::core::Observable<ii::gui::WidgetColor> strongShadow{ii::gui::WidgetColor{0,0,0,26./255.}};
    std::vector<bool> events;
    void changed(bool value){events.push_back(value);}
};
