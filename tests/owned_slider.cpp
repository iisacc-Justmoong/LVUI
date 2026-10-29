#include <LVUI/Slider.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double left,double right){return std::abs(left-right)<1e-9;}
void model(){
    lvui::Slider slider;auto initial=slider.configuration();
    require(initial.minimum==0&&initial.maximum==1&&initial.value==.5&&initial.width==320&&initial.height==32&&
        initial.thumb_diameter==18&&initial.track_height==6&&initial.range_inset==9,"Slider defaults changed");
    auto config=initial;config.minimum=-10;config.maximum=10;config.value=1;config.step=2;
    require(slider.set_configuration(config)&&near(slider.value(),2),"Slider failed to snap its initial value");
    require(slider.set_value(-100)&&near(slider.value(),-10),"Slider did not clamp its minimum");
    require(slider.set_value(100)&&near(slider.value(),10),"Slider did not clamp its maximum");
    require(slider.set_value(1)&&near(slider.value(),2),"Slider step rounding is incorrect");
    auto geometry=slider.geometry(100,32);require(geometry&&near(geometry->track.x,9)&&near(geometry->track.width,82)&&
        near(geometry->thumb.x,9+82*.6-9)&&near(geometry->fill.width,82*.6),"Slider geometry does not follow its normalized value");
    require(slider.set_from_position(9,100)&&near(slider.value(),-10),"Slider did not map the left endpoint");
    require(slider.set_from_position(91,100)&&near(slider.value(),10),"Slider did not map the right endpoint");
    require(slider.nudge(-1)&&near(slider.value(),8),"Slider keyboard step is incorrect");
    config.enabled=false;config.value=4;require(slider.set_configuration(config),"Valid disabled Slider was rejected");
    require(!slider.set_from_position(9,100)&&!slider.nudge(1)&&near(slider.value(),4),"Disabled Slider accepted input");
    const auto before=slider.configuration();config.maximum=config.minimum;
    require(!slider.set_configuration(config)&&slider.configuration()==before,"Invalid Slider range replaced valid state");
    config=before;config.value=std::numeric_limits<double>::quiet_NaN();
    require(!slider.set_configuration(config)&&!slider.set_value(std::numeric_limits<double>::quiet_NaN()),"Non-finite Slider value was accepted");
    config=before;config.accessibility_name=std::string("bad\0name",8);
    require(!slider.set_configuration(config)&&slider.configuration()==before,"Invalid Slider accessibility name was accepted");
    config=before;config.height=1;
    require(!slider.set_configuration(config)&&slider.configuration()==before,"A Slider shorter than its thumb was accepted");
    config=before;config.enabled=true;config.minimum=0;config.maximum=1;config.step=0;config.value=.5;
    require(slider.set_configuration(config)&&slider.nudge(1)&&near(slider.value(),.51),"Continuous Slider keyboard step is incorrect");
    auto compact=slider.geometry(8,12);require(compact&&near(compact->track.width,0)&&std::isfinite(compact->thumb.x),"Compact Slider geometry is invalid");
    require(!slider.geometry(std::numeric_limits<double>::infinity(),32),"Non-finite Slider geometry was accepted");
}
}
int main(){model();}
