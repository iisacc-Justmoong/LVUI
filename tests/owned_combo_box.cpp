#include <LVUI/ComboBox.h>
#include <limits>
#include <stdexcept>
#include <string>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    lvui::ComboBox box;const auto c=box.configuration();
    require(c.width==97&&c.height==20&&c.corner_radius==5&&c.arrow==lvui::StepperArrow::up_down,
        "Default ComboBox parameters changed");
    const auto geometry=box.geometry(97,20);require(geometry&&geometry->label.x==8&&geometry->label.width==70&&
        geometry->indicator.x==78&&geometry->indicator.y==1&&geometry->indicator.width==18,
        "ComboBox label and indicator geometry is incorrect");
    require(!box.geometry(-1,20)&&!box.geometry(std::numeric_limits<double>::infinity(),20),
        "Invalid ComboBox extent was accepted");
    require(box.visual({}).background==c.palette.surface&&box.visual({.hovered=true}).background==c.palette.hover&&
        box.visual({.pressed=true}).background==c.palette.pressed,"ComboBox state colors are incorrect");
    auto changed=c;changed.tone=lvui::ButtonTone::borderless;changed.arrow=lvui::StepperArrow::down;
    require(box.set_configuration(changed)&&box.visual({}).background.alpha==0&&box.glyph()=="⌄",
        "Borderless ComboBox or its indicator is incorrect");
    changed.enabled=false;require(box.set_configuration(changed)&&!box.visual({.focused=true}).focused&&
        box.visual({}).text==c.palette.disabled_text,"Disabled ComboBox state is incorrect");
    auto bad=changed;bad.width=18;require(!box.set_configuration(bad)&&box.configuration()==changed,
        "Invalid ComboBox update replaced valid state");
    bad=changed;bad.tone=lvui::ButtonTone::destructive;require(!box.set_configuration(bad),"Unsupported tone was accepted");
    bad=changed;bad.arrow=static_cast<lvui::StepperArrow>(99);require(!box.set_configuration(bad),"Unknown arrow was accepted");
    bad=changed;bad.palette.text.alpha=2;require(!box.set_configuration(bad),"Invalid color was accepted");
}
