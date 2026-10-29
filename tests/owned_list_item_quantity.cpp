#include <LVUI/ListItemQuantity.h>

#include <limits>
#include <stdexcept>

namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    using namespace lvui;
    ListItemQuantity value;auto c=value.configuration();
    c.value=2;c.minimum=1;c.maximum=3;c.step=.5;require(value.set_configuration(c),"Valid quantity was rejected");
    require(value.step(1)==2.5&&value.step(1)==3&&!value.step(1)&&value.step(-1)==2.5,"Quantity did not clamp or suppress a no-op");
    c=value.configuration();c.enabled=false;require(value.set_configuration(c)&&!value.step(1),"Disabled quantity changed");
    const auto previous=value.configuration();c.step=0;require(!value.set_configuration(c)&&value.configuration()==previous,"Invalid step replaced the state");
    c=previous;c.minimum=4;require(!value.set_configuration(c),"Inverted bounds were accepted");
    c=previous;c.maximum=std::numeric_limits<double>::quiet_NaN();require(!value.set_configuration(c),"NaN bound was accepted");
    c=previous;c.enabled=true;c.minimum=0;c.maximum=std::numeric_limits<double>::infinity();
    c.value=std::numeric_limits<double>::max();c.step=std::numeric_limits<double>::max();
    require(value.set_configuration(c)&&!value.step(1),"Unbounded increment produced infinity");
    require(!value.step(2),"Unknown step direction was accepted");
}
