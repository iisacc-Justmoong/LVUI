#include <LVUI/CheckBox.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-9;}
}
int main(){
    lvui::CheckBox box;
    lvui::CheckBoxConfiguration value;value.text="Label";
    require(box.set_configuration(value)&&!box.checked(),"Initial checkbox configuration failed");
    auto geometry=box.geometry(57,18,33,13);
    require(geometry&&near(geometry->box.x,.5)&&near(geometry->box.y,.5)&&
            near(geometry->box.width,17)&&near(geometry->text.x,23.5)&&
            near(geometry->text.y,2.5)&&near(geometry->radius,3.5),"Checkbox geometry differs from its design");
    require(box.visual({}).fill==value.palette.unchecked && !box.visual({}).draw_mark,
            "Unchecked appearance is incorrect");
    auto changed=box.activate();
    require(changed&&*changed&&box.checked()&&box.visual({}).draw_mark&&
            box.visual({}).fill==value.palette.checked,"Activation did not select the checkbox");
    require(box.visual({.pressed=true}).fill.red<box.visual({.hovered=true}).fill.red,
            "Pressed appearance did not take precedence");
    require(box.mark_coverage(.44,.67)>0&&box.mark_coverage(.05,.05)==0,
            "The independent check mark geometry is incorrect");
    value.checked=true;value.enabled=false;
    require(box.set_configuration(value)&&!box.activate()&&box.checked()&&
            box.visual({}).fill==value.palette.disabled_checked,"Disabled checkbox admitted activation");
    auto before=box.configuration(),invalid=value;invalid.box_size=0;
    require(!box.set_configuration(invalid)&&box.configuration()==before,"Invalid size changed the checkbox");
    invalid=value;invalid.mark_stroke_width=std::numeric_limits<double>::infinity();
    require(!box.set_configuration(invalid),"Invalid mark width was accepted");
    invalid=value;invalid.palette.checked.red=2;
    require(!box.set_configuration(invalid),"Invalid color was accepted");
    invalid=value;invalid.text=std::string("x\0y",3);
    require(!box.set_configuration(invalid),"Invalid text was accepted");
    require(!box.geometry(-1,18,3,13),"Invalid allocated extent was accepted");
}
