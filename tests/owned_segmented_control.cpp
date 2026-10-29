#include <LVUI/SegmentedControl.h>
#include <limits>
#include <stdexcept>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    lvui::SegmentedControl row;auto original=row.configuration();
    require(original.horizontal_padding==4&&original.vertical_padding==3.5&&original.spacing==2&&
        original.border_width==2&&original.corner_radius==12&&original.force_borderless_tone,
        "Segmented control defaults changed");
    auto rect=row.geometry(100,30);require(rect&&rect->radius==12&&rect->border_width==2,
        "Round segmented surface geometry is incorrect");
    auto capsule=original;capsule.shape=lvui::SegmentedShape::cylinder;
    require(row.set_configuration(capsule),"Capsule configuration was rejected");
    auto cylinder=row.geometry(100,30);require(cylinder&&cylinder->radius==15,"Capsule did not use half its height");
    auto narrow=row.geometry(8,30);require(narrow&&narrow->radius==4&&narrow->border_width==2,
        "Capsule geometry exceeded its width");
    require(!row.geometry(-1,30)&&!row.geometry(std::numeric_limits<double>::infinity(),30),"Invalid geometry was accepted");
    auto invalid=capsule;invalid.spacing=-1;require(!row.set_configuration(invalid)&&row.configuration()==capsule,
        "Invalid segmented configuration replaced the valid state");
    invalid=capsule;invalid.shape=static_cast<lvui::SegmentedShape>(8);require(!row.set_configuration(invalid),"Unknown shape was accepted");
    invalid=capsule;invalid.border.alpha=-.1;require(!row.set_configuration(invalid),"Invalid border color was accepted");
}
