#include <LVUI/Slider.h>
#include <iiGUI/SliderModel.h>
namespace lvui {using Slider=SliderBinding<ii::gui::BasicSlider>;} 
#include <cmath>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-9;}

void filled_geometry(){
    using lvui::SliderSize;
    for(const auto& [size,height,track,thumb,shown]:{
        std::tuple{SliderSize::mini,22.,12.,8.,false},
        std::tuple{SliderSize::small,24.,16.,8.,false},
        std::tuple{SliderSize::regular,32.,28.,20.,true},
        std::tuple{SliderSize::large,44.,44.,36.,true}}){
        lvui::SliderConfiguration config;
        config.minimum=-5;config.maximum=5;config.value=0;
        lvui::apply_slider_size(config,size);
        lvui::apply_slider_type(config,lvui::SliderType::filled);
        require(config.height==height&&config.track_height==track&&config.thumb_diameter==thumb&&
                    config.range_inset==track/2&&config.show_symbol==shown,
                "Filled slider preset differs from the Figma size");
        require(!config.center_biased&&!config.show_ticks&&config.minimum==-5&&config.maximum==5,
                "Filled slider preset changed its value contract");
        lvui::Slider slider;require(slider.set_configuration(config),"Filled slider preset was rejected");
        auto invalid=config;invalid.palette.inactive_fill.alpha=std::numeric_limits<double>::quiet_NaN();
        require(!slider.set_configuration(invalid)&&slider.configuration()==config,"Invalid inactive fill replaced the valid slider state");
        auto middle=slider.geometry(320,height);
        require(middle&&near(middle->track.x,0)&&near(middle->track.width,320)&&
                    near(middle->track.y,(height-track)/2)&&near(middle->track.height,track)&&
                    near(middle->fill.x,0)&&near(middle->fill.width,160+track/2)&&
                    near(middle->thumb.x,160-thumb/2)&&near(middle->thumb.y,(height-thumb)/2),
                "Filled slider midpoint geometry differs from Figma");
        require(slider.set_from_position(track/2,320)&&near(slider.value(),-5),"Filled slider minimum input is wrong");
        auto minimum=slider.geometry(320,height);
        require(minimum&&near(minimum->fill.x,0)&&near(minimum->fill.width,track),"Filled minimum capsule is wrong");
        require(slider.set_from_position(320-track/2,320)&&near(slider.value(),5),"Filled slider maximum input is wrong");
        auto maximum=slider.geometry(320,height);
        require(maximum&&near(maximum->fill.x,0)&&near(maximum->fill.width,320),"Filled maximum capsule is wrong");
        auto reverse=config;reverse.right_to_left=true;
        require(slider.set_configuration(reverse),"Mirrored filled slider was rejected");
        middle=slider.geometry(320,height);
        require(middle&&near(middle->fill.x,160-track/2)&&near(middle->fill.width,160+track/2),
                "Mirrored filled slider geometry is wrong");
        require(slider.set_from_position(320-track/2,320)&&near(slider.value(),-5),"Mirrored minimum input is wrong");
        require(slider.set_from_position(track/2,320)&&near(slider.value(),5),"Mirrored maximum input is wrong");
        lvui::SliderConfiguration other;other.minimum=-5;other.maximum=5;other.value=0;
        lvui::apply_slider_type(other,lvui::SliderType::filled);lvui::apply_slider_size(other,size);
        require(other.height==config.height&&other.track_height==config.track_height&&
                    other.thumb_diameter==config.thumb_diameter&&other.range_inset==config.range_inset&&
                    other.show_symbol==config.show_symbol,
                "Filled type and size preset order changed the result");
    }
}
void labeled_and_segmented(){
    using namespace lvui;
    require(slider_type(5)==SliderType::min_max_labels&&slider_type(6)==SliderType::segmented,
            "Labeled and segmented types lost their public numeric values");
    for(const auto size:{SliderSize::mini,SliderSize::small,SliderSize::regular,SliderSize::large}){
        SliderConfiguration labeled;apply_slider_size(labeled,size);apply_slider_type(labeled,SliderType::min_max_labels);
        require(labeled.show_min_max&&!labeled.show_ticks&&!labeled.show_symbol&&
                labeled.range_inset==labeled.track_height/2,"Min-max labels preset is not the labeled filled variant");
        SliderConfiguration segmented;apply_slider_type(segmented,SliderType::segmented);apply_slider_size(segmented,size);
        require(segmented.show_min_max&&segmented.show_ticks&&!segmented.show_symbol&&segmented.segment_count==5&&
                segmented.height==labeled.height&&segmented.track_height==labeled.track_height&&
                segmented.thumb_diameter==labeled.thumb_diameter,"Segmented preset differs from Figma");
        apply_slider_type(segmented,SliderType::normal);
        require(!segmented.show_min_max&&!segmented.show_ticks&&!segmented.show_symbol,
                "Leaving a labeled variant retained its preset decorations");
    }
    SliderConfiguration config;config.minimum=-2;config.maximum=2;config.value=.3;
    apply_slider_type(config,SliderType::segmented);
    Slider slider;require(slider.set_configuration(config)&&near(slider.value(),0),"Segmented value did not snap to its five positions");
    require(slider.set_value(.51)&&near(slider.value(),1),"Segmented value snapped to the wrong position");
    require(slider.nudge(-1)&&near(slider.value(),0)&&slider.nudge(1)&&near(slider.value(),1),
            "Segmented keyboard or accessibility increment did not use its interval");
    config.segment_count=3;config.value=.8;
    require(slider.set_configuration(config)&&near(slider.value(),0)&&slider.nudge(1)&&near(slider.value(),2),
            "Changing segment count did not update snapping and increments");
    auto invalid=config;invalid.segment_count=1;
    require(!slider.set_configuration(invalid)&&near(slider.value(),2),"Invalid segment count replaced the valid state");
    invalid.segment_count=513;require(!slider.set_configuration(invalid),"Too many segments were accepted");
    config.segment_count=5;config.step=.25;config.value=.3;
    require(slider.set_configuration(config)&&near(slider.value(),.25)&&slider.nudge(1)&&near(slider.value(),.5),
            "Explicit step did not override the automatic segment interval");
    config.step=0;config.automatic_segment_step=false;config.value=.3;
    require(slider.set_configuration(config)&&near(slider.value(),.3)&&slider.nudge(1)&&near(slider.value(),.34),
            "Explicit zero step did not preserve continuous input");
}
}
int main(){filled_geometry();labeled_and_segmented();}
