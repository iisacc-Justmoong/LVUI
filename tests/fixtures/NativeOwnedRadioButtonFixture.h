#pragma once
#include <LVUI/Motion.h>
#include <iiGUI/NativeWidgets.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>
struct NativeOwnedRadioChild final {};
struct NativeOwnedRadioRow final {
    std::string id,title;bool selected=false;double box_size=17;
    friend bool operator==(const NativeOwnedRadioRow&,const NativeOwnedRadioRow&) = default;
};
struct NativeOwnedRadioRows final {
    ii::core::Observable<std::vector<NativeOwnedRadioRow>> items{
        std::vector<NativeOwnedRadioRow>{{"a","A",false,17},{"b","B",false,17}}};
};
struct NativeOwnedRadioButtonModel final {
    ii::core::Observable<std::string> text{std::string("First")},group{std::string("choice")};
    ii::core::Observable<bool> first{true},enabled{true},show_child{false},motion_enabled{false};
    ii::core::Observable<double> candidate_size{17};
    ii::core::Observable<ii::gui::WidgetColor> on_color{ii::gui::WidgetColor{.0812049285,.5099760294,1,1}};
    std::vector<std::string> events;
    void first_changed(bool value){events.push_back(value?"first:true":"first:false");}
    void second_changed(bool value){events.push_back(value?"second:true":"second:false");}
};
