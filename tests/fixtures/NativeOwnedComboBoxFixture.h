#pragma once
#include <LVUI/ComboBox.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <string>
struct NativeOwnedComboBoxModel final {
    ii::core::Observable<std::string> text{"Initial"};
    ii::core::Observable<lvui::StepperArrow> arrow{lvui::StepperArrow::up_down};
    ii::core::Observable<lvui::ButtonTone> tone{lvui::ButtonTone::primary};
    ii::core::Observable<bool> enabled{true};
    ii::core::Observable<double> width{97};
    unsigned clicks=0,presses=0,releases=0;
    void clicked(){++clicks;}
    void pressed(){++presses;}
    void released(){++releases;}
};
