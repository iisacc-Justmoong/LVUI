#pragma once
#include <LVUI/Stepper.h>
#include <iiSDKCore/Core/Property/Observable.h>
#include <vector>
#include <string>

struct NativeOwnedStepperModel final {
    ii::core::Observable<lvui::StepperArrow> arrow{lvui::StepperArrow::up_down};
    ii::core::Observable<lvui::ButtonTone> tone{lvui::ButtonTone::primary};
    ii::core::Observable<bool> enabled{true};
    ii::core::Observable<double> radius{4};
    std::vector<int> steps;
    unsigned clicks=0,presses=0,releases=0;
    std::vector<std::string> events;
    void stepped(int value){steps.push_back(value);events.emplace_back("stepped");}
    void clicked(){++clicks;events.emplace_back("clicked");}
    void pressed(){++presses;events.emplace_back("pressed");}
    void released(){++releases;events.emplace_back("released");}
    void method(){events.emplace_back("method");}
    void after(){events.emplace_back("after");}
    void last(){events.emplace_back("last");}
};
