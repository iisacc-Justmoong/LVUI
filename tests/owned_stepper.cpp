#include <LVUI/Stepper.h>
#include <limits>
#include <stdexcept>

namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    lvui::Stepper stepper;auto options=stepper.configuration();
    require(options.width==18&&options.height==18&&options.corner_radius==4&&stepper.accepts(1)&&stepper.accepts(-1),
        "Stepper default dimensions or directions changed");
    require(stepper.direction_at(2,18)==1&&stepper.direction_at(16,18)==-1&&!stepper.direction_at(-1,18),
        "Stepper pointer direction is not bounded by its surface");
    options.arrow=lvui::StepperArrow::up;require(stepper.set_configuration(options)&&stepper.accepts(1)&&!stepper.accepts(-1)&&stepper.direction_at(17,18)==1,
        "Up-only Stepper accepted the wrong direction");
    options.arrow=lvui::StepperArrow::down;require(stepper.set_configuration(options)&&!stepper.accepts(1)&&stepper.accepts(-1),
        "Down-only Stepper accepted the wrong direction");
    options.enabled=false;require(stepper.set_configuration(options)&&!stepper.direction_at(5,18),"Disabled Stepper accepted pointer input");
    const auto previous=stepper.configuration();options.height=0;
    require(!stepper.set_configuration(options)&&stepper.configuration()==previous,"Invalid Stepper dimensions replaced valid state");
    options=previous;options.corner_radius=std::numeric_limits<double>::quiet_NaN();
    require(!stepper.set_configuration(options),"Non-finite Stepper radius was accepted");
    options=previous;options.arrow=static_cast<lvui::StepperArrow>(99);
    require(!stepper.set_configuration(options),"Unknown Stepper direction was accepted");
    options=previous;options.enabled=true;options.tone=lvui::ButtonTone::borderless;
    require(stepper.set_configuration(options)&&stepper.visual({}).background.alpha==0,"Borderless Stepper has an idle fill");
    options.hover_color=lvui::StyleColor{.4,.2,.1,1};options.tone=lvui::ButtonTone::destructive;
    require(stepper.set_configuration(options)&&stepper.visual({.hovered=true}).background==*options.hover_color,
        "Explicit hover color did not apply to the selected tone");
}
