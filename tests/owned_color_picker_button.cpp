#include <LVUI/ColorPickerButton.h>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    lvui::ColorPickerButton button;auto configuration=button.configuration();auto geometry=button.geometry(28,28);
    require(geometry&&geometry->ring.width==20&&std::abs(geometry->swatch.width-20*18./28)<1e-10&&geometry->radius==4,
        "Color button design geometry changed");
    require(configuration.current_color.red==.5275065302848816&&configuration.show_hue_ring&&configuration.size==28,
        "Color button defaults changed");
    configuration.circular=true;configuration.enabled=false;configuration.border_opacity=.5;
    require(button.set_configuration(configuration),"Valid color button configuration was rejected");
    geometry=button.geometry(36,22);const auto disabled=button.visual({});
    require(geometry&&geometry->radius==11&&geometry->ring.width==14&&disabled.opacity==.32&&disabled.border.alpha==.1,
        "Disabled or circular color button geometry changed");
    const auto previous=button.configuration();auto invalid=previous;invalid.current_color.blue=std::numeric_limits<double>::quiet_NaN();
    require(!button.set_configuration(invalid)&&button.configuration()==previous,"Invalid color replaced the existing button");
    invalid=previous;invalid.size=0;require(!button.set_configuration(invalid),"Invalid color button size was accepted");
    invalid=previous;invalid.accessibility_name=std::string("bad\0name",8);require(!button.set_configuration(invalid),"Invalid accessible name was accepted");
    require(!button.geometry(-1,3),"Invalid color button extent was accepted");
}
