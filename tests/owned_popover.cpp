#include <LVUI/Popover.h>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    lvui::Popover model;auto config=model.configuration();
    require(config.padding==12&&config.margins==8&&config.corner_radius==12&&config.surface_opacity==.25,
        "Popover default design values changed");
    auto natural=model.natural_size({100,50});require(natural&&natural->width==124&&natural->height==74,"Popover natural size is incorrect");
    config.width=200;config.height=100;require(model.set_configuration(config),"Explicit popover geometry was rejected");
    natural=model.natural_size({100,50});require(natural&&natural->width==200&&natural->height==100,"Explicit popover geometry was ignored");
    const auto previous=model.configuration();config.surface_opacity=std::numeric_limits<double>::quiet_NaN();
    require(!model.set_configuration(config)&&model.configuration()==previous,"Invalid popover opacity changed state");
    config=previous;config.accessibility_name=std::string("bad\0name",8);require(!model.set_configuration(config),"Invalid popover name was accepted");
    require(!model.natural_size({-1,50})&&!model.natural_size({9000,50}),"Invalid content size was accepted");
}
