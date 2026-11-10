#pragma once
#include <LVUI/ListItem.h>
#include <LVUI/Stepper.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>

struct NativeOwnedListItemDetailControlsModel final {
    ii::core::Observable<double> quantity{2};
    ii::core::Observable<int> unit_index{0},segment_index{0};
    ii::core::Observable<bool> show_segments{true};
    ii::core::Observable<lvui::ListItemVariant> kind{lvui::ListItemVariant::detail_settings};
    ii::core::Observable<lvui::StepperArrow> arrow{lvui::StepperArrow::up_down};
    ii::core::Observable<lvui::ButtonTone> tone{lvui::ButtonTone::primary};
    ii::core::Observable<std::vector<std::string>> units{{"px","cm"}},segments{{"Original","Edited"}};
    ii::core::Observable<std::vector<std::string>> formats{std::vector<std::string>{}};
    std::vector<std::string> events;
    void quantityChanged(double value){quantity.set(value);events.push_back("quantity:"+std::to_string(int(value)));}
    void unitChanged(int value){unit_index.set(value);events.push_back("unit:"+std::to_string(value));}
    void segmentChanged(int value){segment_index.set(value);events.push_back("segment:"+std::to_string(value));}
};
