#pragma once
#include <LVUI/ComboBox.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
#include <vector>
struct NativeOwnedComboBoxModel final {
    ii::core::Observable<std::string> text{"Initial"};
    ii::core::Observable<lvui::StepperArrow> arrow{lvui::StepperArrow::up_down};
    ii::core::Observable<lvui::ButtonTone> tone{lvui::ButtonTone::primary};
    ii::core::Observable<bool> enabled{true};
    ii::core::Observable<double> width{97};
    unsigned clicks=0,presses=0,releases=0;
    std::vector<std::string> events;
    void clicked(){++clicks;events.emplace_back("clicked");}
    void pressed(){++presses;events.emplace_back("pressed");}
    void released(){++releases;events.emplace_back("released");}
    void method(){events.emplace_back("method");}
    void after(){events.emplace_back("after");}
    void last(){events.emplace_back("last");}
};
