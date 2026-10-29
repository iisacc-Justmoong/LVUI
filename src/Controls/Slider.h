#pragma once
#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
struct SliderPalette final {
    StyleColor track{.1935568452,.1978072077,.1999323964,1};
    StyleColor fill{.0812049285,.5099760294,1,1};
    StyleColor thumb{1,1,1,1};
    StyleColor focus{.0812049285,.5099760294,1,1};
    friend bool operator==(const SliderPalette&,const SliderPalette&)=default;
};
struct SliderConfiguration final {
    double minimum=0,maximum=1,value=.5,step=0;
    double width=320,height=32,thumb_diameter=18,track_height=6,range_inset=9;
    bool enabled=true;
    SliderPalette palette;
    std::string accessibility_name="Slider";
    friend bool operator==(const SliderConfiguration&,const SliderConfiguration&)=default;
};
struct SliderRect final {double x=0,y=0,width=0,height=0;};
struct SliderGeometry final {SliderRect track,fill,thumb;};

// Range policy and geometry belong to LVUI. Input delivery, painting and
// accessibility transport are replaceable host services.
class Slider final {
    SliderConfiguration configuration_;
    [[nodiscard]] static double resolved_value(const SliderConfiguration& config,double raw)noexcept {
        auto value=std::clamp(raw,config.minimum,config.maximum);
        if(config.step<=0||value<=config.minimum||value>=config.maximum)return value;
        const auto steps=(value-config.minimum)/config.step;
        // Above exact integer precision the step is smaller than the value's
        // representable spacing, so rounding cannot improve the result.
        if(!std::isfinite(steps)||steps>9007199254740991.)return value;
        return std::clamp(config.minimum+std::round(steps)*config.step,config.minimum,config.maximum);
    }
public:
    [[nodiscard]] static bool validate(const SliderConfiguration& value)noexcept {
        const auto range=[](double n,double low,double high){return std::isfinite(n)&&n>=low&&n<=high;};
        const auto color=[&](StyleColor c){return range(c.red,0,1)&&range(c.green,0,1)&&range(c.blue,0,1)&&range(c.alpha,0,1);};
        return range(value.minimum,-1e12,1e12)&&range(value.maximum,-1e12,1e12)&&value.minimum<value.maximum&&
            range(value.value,-1e12,1e12)&&range(value.step,0,32767)&&range(value.width,18,8192)&&
            range(value.height,1,8192)&&range(value.thumb_diameter,1,256)&&range(value.track_height,1,256)&&
            value.width>=value.thumb_diameter&&value.height>=value.thumb_diameter&&value.height>=value.track_height&&
            range(value.range_inset,0,128)&&color(value.palette.track)&&color(value.palette.fill)&&
            color(value.palette.thumb)&&color(value.palette.focus)&&valid_accessibility_text(value.accessibility_name);
    }
    bool set_configuration(SliderConfiguration value) {
        if(!validate(value))return false;
        value.value=resolved_value(value,value.value);configuration_=std::move(value);return true;
    }
    [[nodiscard]] const SliderConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] double value()const noexcept{return configuration_.value;}
    [[nodiscard]] double normalized()const noexcept{
        return (configuration_.value-configuration_.minimum)/(configuration_.maximum-configuration_.minimum);
    }
    bool set_value(double value)noexcept {
        if(!std::isfinite(value))return false;
        configuration_.value=resolved_value(configuration_,value);return true;
    }
    bool set_from_position(double x,double width)noexcept {
        if(!configuration_.enabled||!std::isfinite(x)||!std::isfinite(width)||width<0||width>8192)return false;
        const auto inset=std::min(configuration_.range_inset,width/2),length=width-2*inset;
        if(length<=0)return true;
        const auto amount=std::clamp((x-inset)/length,0.,1.);
        return set_value(configuration_.minimum+amount*(configuration_.maximum-configuration_.minimum));
    }
    bool nudge(int direction)noexcept {
        if(!configuration_.enabled||direction==0)return false;
        const auto step=configuration_.step>0?configuration_.step:(configuration_.maximum-configuration_.minimum)/100;
        return set_value(configuration_.value+(direction>0?step:-step));
    }
    [[nodiscard]] std::optional<SliderGeometry> geometry(double width,double height)const noexcept {
        if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192)return std::nullopt;
        const auto inset=std::min(configuration_.range_inset,width/2),length=std::max(0.,width-2*inset);
        const auto track_y=(height-configuration_.track_height)/2,thumb_y=(height-configuration_.thumb_diameter)/2;
        const auto fill=length*normalized();
        return SliderGeometry{{inset,track_y,length,configuration_.track_height},{inset,track_y,fill,configuration_.track_height},
            {inset+fill-configuration_.thumb_diameter/2,thumb_y,configuration_.thumb_diameter,configuration_.thumb_diameter}};
    }
};
} // namespace lvui
