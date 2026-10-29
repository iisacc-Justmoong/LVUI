#pragma once
#include <LVUI/Stepper.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <vector>

struct NativeOwnedStepperModel final {
    ii::core::Observable<lvui::StepperArrow> arrow{lvui::StepperArrow::up_down};
    ii::core::Observable<lvui::ButtonTone> tone{lvui::ButtonTone::primary};
    ii::core::Observable<bool> enabled{true};
    ii::core::Observable<double> radius{4};
    std::vector<int> steps;
    unsigned clicks=0,presses=0,releases=0;
    void stepped(int value){steps.push_back(value);}
    void clicked(){++clicks;}
    void pressed(){++presses;}
    void released(){++releases;}
};
