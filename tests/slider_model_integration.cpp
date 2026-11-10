#include <LVUI/Slider.h>
#include <iiGUI/SliderModel.h>
namespace lvui {using Slider=SliderBinding<ii::gui::BasicSlider>;} 
#include <cmath>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double left,double right){return std::abs(left-right)<1e-9;}
void model(){
    lvui::Slider slider;auto initial=slider.configuration();
    require(initial.minimum==0&&initial.maximum==1&&initial.value==.5&&initial.width==320&&initial.height==32&&
        initial.thumb_diameter==18&&initial.track_height==6&&initial.range_inset==9,"Slider defaults changed");
    require(initial.palette.thumb==lvui::figma_defaults::text_title_header_color,
        "Slider thumb does not use the LVUI title text token");
    require(initial.palette.track==lvui::figma_defaults::panel_surface_solid,
        "Slider track does not use the original LVUI surfaceSolid token");
    require(!initial.show_min_max&&initial.show_labels&&!initial.show_endpoint_icons&&!initial.right_to_left&&
            initial.minimum_label=="0%"&&initial.maximum_label=="100%","Slider endpoint defaults changed");
    require(!initial.show_ticks&&initial.tick_count==11&&!initial.show_symbol&&initial.symbol_name=="sliderSymbolSun",
        "Slider decoration defaults changed");
    for(double invalid:{1.,513.,3.5,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        bool rejected=false;try{(void)lvui::slider_tick_count(invalid);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"Invalid Slider tick count was accepted");
    }
    require(lvui::slider_tick_count(2)==2&&lvui::slider_tick_count(512)==512,"Valid Slider tick count was rejected");
    auto bad_symbol=initial;bad_symbol.symbol_name=" ";require(!slider.set_configuration(bad_symbol),"Blank Slider symbol name was accepted");
    for(const auto& [size,height,thumb,track,inset]:{
        std::tuple{lvui::SliderSize::mini,22.,12.,4.,8.},
        std::tuple{lvui::SliderSize::small,24.,14.,6.,8.},
        std::tuple{lvui::SliderSize::regular,32.,18.,6.,9.},
        std::tuple{lvui::SliderSize::large,44.,22.,8.,11.}}){
        auto preset=initial;lvui::apply_slider_size(preset,size);
        require(preset.height==height&&preset.thumb_diameter==thumb&&preset.track_height==track&&preset.range_inset==inset,
            "Slider size preset differs from the Figma geometry");
        require(preset.width==initial.width&&preset.value==initial.value,"Slider size preset changed its range or width");
    }
    bool rejected=false;try{(void)lvui::slider_size(4);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Unknown reactive Slider size was accepted");
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

    config=initial;config.width=320;config.show_min_max=true;require(slider.set_configuration(config),"Valid Slider endpoints were rejected");
    const lvui::SliderEndpointWidths measured{20,34};auto layout=slider.endpoint_layout(320,32,measured);
    require(layout&&near(layout->minimum_label.x,0)&&near(layout->minimum_label.width,20)&&
        near(layout->maximum_label.x,286)&&near(layout->maximum_label.width,34)&&
        near(layout->control.x,28)&&near(layout->control.width,250),"Slider labels did not reserve the Figma control width");
    geometry=slider.geometry(320,32,measured);require(geometry&&near(geometry->track.x,37)&&near(geometry->track.width,232)&&
        near(geometry->fill.width,116),"Slider track did not use its endpoint-adjusted range");
    require(slider.set_from_position(37,320,measured)&&near(slider.value(),0)&&
        slider.set_from_position(269,320,measured)&&near(slider.value(),1),"Slider endpoints changed pointer mapping");
    config.right_to_left=true;require(slider.set_configuration(config),"RTL Slider was rejected");
    layout=slider.endpoint_layout(320,32,measured);require(layout&&near(layout->maximum_label.x,0)&&
        near(layout->minimum_label.x,300)&&near(layout->control.x,42)&&near(layout->control.width,250),
        "RTL Slider did not mirror its endpoint labels");
    require(slider.set_from_position(283,320,measured)&&near(slider.value(),0)&&
        slider.set_from_position(51,320,measured)&&near(slider.value(),1),"RTL Slider pointer mapping was not mirrored");
    require(slider.set_value(.25),"RTL Slider rejected a valid value");
    geometry=slider.geometry(320,32,measured);require(geometry&&near(geometry->track.x,51)&&
        near(geometry->fill.x,225)&&near(geometry->fill.width,58)&&near(geometry->thumb.x,216),
        "RTL Slider fill and thumb did not follow the reversed value direction");
    config.show_endpoint_icons=true;require(slider.set_configuration(config),"Slider endpoint icons were rejected");
    layout=slider.endpoint_layout(320,32,measured);require(layout&&near(layout->minimum_icon.width,18)&&
        near(layout->maximum_icon.width,18)&&layout->minimum_icon.x>layout->minimum_label.x,
        "RTL Slider endpoint icon order or size changed");
    layout=slider.endpoint_layout(18,22,{10000,10000});require(layout&&layout->control.width>=0&&
        layout->control.x>=0&&layout->control.x+layout->control.width<=18&&
        layout->minimum_icon.width==0&&layout->maximum_icon.width==0,"Narrow Slider overflowed its allocated width");
    require(!slider.endpoint_layout(320,32,{std::numeric_limits<double>::quiet_NaN(),0}),"Invalid endpoint measurement was accepted");
    config.right_to_left=false;config.show_endpoint_icons=false;config.minimum_label.clear();
    require(slider.set_configuration(config),"Slider with one empty label was rejected");
    layout=slider.endpoint_layout(320,32,measured);require(layout&&layout->minimum_label.width==0&&
        near(layout->control.x,0)&&near(layout->maximum_label.x,286),"An empty endpoint reserved unnecessary space");
    config.show_labels=false;require(slider.set_configuration(config),"Slider with hidden labels was rejected");
    layout=slider.endpoint_layout(320,32,measured);require(layout&&near(layout->control.x,0)&&near(layout->control.width,320),
        "Hidden endpoint labels reduced the usable range");
    config.minimum_icon_name=" ";require(!slider.set_configuration(config),"Blank endpoint icon name was accepted");
}
void centered(){
    lvui::Slider slider;lvui::SliderConfiguration value;
    lvui::apply_slider_type(value,lvui::SliderType::center_biased_ticks);
    require(value.center_biased&&value.show_ticks,"Center-biased tick preset was not applied");
    value.minimum=-5;value.maximum=5;value.value=0;
    require(slider.set_configuration(value),"Center-biased Slider was rejected");
    auto middle=slider.geometry(320,32);
    require(middle&&near(middle->fill.width,0)&&near(middle->center_mark.x,159)&&
        near(middle->center_mark.y,11)&&near(middle->center_mark.width,2)&&near(middle->center_mark.height,10),
        "Center-biased Slider did not match the Figma midpoint and mark");
    require(slider.set_value(-2.5),"Center-biased Slider rejected a valid value");
    auto left=slider.geometry(320,32);
    require(left&&near(left->fill.x,84.5)&&near(left->fill.width,75.5),"Center-biased fill did not extend left from the midpoint");
    require(slider.set_value(2.5),"Center-biased Slider rejected a positive value");
    auto right=slider.geometry(320,32);
    require(right&&near(right->fill.x,160)&&near(right->fill.width,75.5),"Center-biased fill did not extend right from the midpoint");
    require(slider.set_value(-5),"Center-biased Slider rejected its minimum");
    left=slider.geometry(320,32);
    require(left&&near(left->fill.x,9)&&near(left->fill.width,151),"Center-biased minimum did not fill from the endpoint to the midpoint");
    require(slider.set_value(5),"Center-biased Slider rejected its maximum");
    right=slider.geometry(320,32);
    require(right&&near(right->fill.x,160)&&near(right->fill.width,151),"Center-biased maximum did not fill from the midpoint to the endpoint");
    value=slider.configuration();value.right_to_left=true;value.value=-2.5;
    require(slider.set_configuration(value),"Mirrored center-biased Slider was rejected");
    right=slider.geometry(320,32);
    require(right&&near(right->fill.x,160)&&near(right->fill.width,75.5)&&near(right->center_mark.x,159),
        "Mirrored Slider did not retain the physical midpoint");
    require(slider.set_from_position(311,320)&&near(slider.value(),-5),"Mirrored center-biased input changed its range mapping");
    auto invalid=value;invalid.palette.mark.alpha=std::numeric_limits<double>::quiet_NaN();
    require(!slider.set_configuration(invalid),"Invalid Slider mark color was accepted");
    const auto before_bad_color=slider.configuration();
    invalid=before_bad_color;invalid.palette.label.red=std::numeric_limits<double>::quiet_NaN();
    require(!slider.set_configuration(invalid)&&slider.configuration()==before_bad_color,"Invalid Slider label color replaced valid state");
    invalid=before_bad_color;invalid.palette.symbol.alpha=std::numeric_limits<double>::quiet_NaN();
    require(!slider.set_configuration(invalid)&&slider.configuration()==before_bad_color,"Invalid Slider symbol color replaced valid state");
    const auto empty=slider.geometry(0,32);
    require(empty&&near(empty->center_mark.width,0),"An empty Slider drew a center mark");
    value.show_min_max=true;value.right_to_left=false;value.value=0;
    require(slider.set_configuration(value),"Center-biased Slider with endpoints was rejected");
    auto endpoints=slider.geometry(320,32,{20,34});
    require(endpoints&&near(endpoints->center_mark.x,152),"Center mark ignored endpoint layout");
    value.right_to_left=true;require(slider.set_configuration(value),"Mirrored Slider with endpoints was rejected");
    endpoints=slider.geometry(320,32,{20,34});
    require(endpoints&&near(endpoints->center_mark.x,166),"Mirrored center mark ignored endpoint layout");
    lvui::apply_slider_type(value,lvui::SliderType::ticks);
    require(!value.center_biased&&value.show_ticks,"Tick Slider preset was not applied");
    lvui::apply_slider_type(value,lvui::SliderType::normal);
    require(!value.center_biased&&!value.show_ticks,"Normal Slider did not restore its decoration defaults");
    bool rejected=false;try{(void)lvui::slider_type(99);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Invalid reactive Slider type was accepted");
}
}
int main(){model();centered();}
