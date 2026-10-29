#pragma once
#include <LVUI/Motion.h>
#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
struct RadioButtonPalette final {
    StyleColor on{.0812049285,.5099760294,1,1},off{1,1,1,.9};
    StyleColor disabled_on{.1935568452,.1978072077,.1999323964,1};
    StyleColor disabled_off{.1935568452,.1978072077,.1999323964,1};
    StyleColor dot{1,1,1,.9},disabled_dot{1,1,1,.5};
    StyleColor text{1,1,1,.8},disabled_text{1,1,1,.3},focus{.0812049285,.5099760294,1,1};
    friend bool operator==(const RadioButtonPalette&,const RadioButtonPalette&) = default;
};
struct RadioButtonConfiguration final {
    std::string text,accessibility_name,group;
    bool enabled=true,checked=false,motion_enabled=true;
    double indicator_size=18,dot_size=8,label_gap=8,font_size=13,line_height=13;
    unsigned font_weight=500;
    Motion motion{.duration=std::chrono::milliseconds{180},.curve=MotionCurve::out_back,.overshoot=1.45};
    RadioButtonPalette palette;
    friend bool operator==(const RadioButtonConfiguration&,const RadioButtonConfiguration&) = default;
};
struct RadioButtonInteraction final {bool enabled=true,focused=false;};
struct RadioButtonVisual final {StyleColor indicator,dot,text,focus;bool draw_dot=false,focused=false;};
struct RadioButtonRect final {double x=0,y=0,width=0,height=0;};
struct RadioButtonGeometry final {RadioButtonRect indicator,dot,text;double indicator_radius=0,dot_radius=0;bool draw_indicator=false,draw_dot=false,draw_text=false;};

class RadioButton final {
public:
    using TimePoint=std::chrono::steady_clock::time_point;
    [[nodiscard]] const RadioButtonConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] bool checked()const noexcept{return configuration_.checked;}
    [[nodiscard]] bool animating()const noexcept{return transition_.active();}
    [[nodiscard]] double dot_scale()const noexcept{
        const auto max_scale=configuration_.dot_size>0?std::max(1.0,configuration_.indicator_size/configuration_.dot_size):1;
        return std::clamp(transition_.value()[0],0.0,max_scale);
    }
    [[nodiscard]] static bool validate(const RadioButtonConfiguration& c)noexcept{
        const auto bound=[](double v,double lo,double hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
        const auto color=[&](StyleColor v){return bound(v.red,0,1)&&bound(v.green,0,1)&&bound(v.blue,0,1)&&bound(v.alpha,0,1);};
        if(!valid_accessibility_text(c.text)||!valid_accessibility_text(c.accessibility_name)||
           !valid_accessibility_text(c.group)||c.group.size()>1024||!bound(c.indicator_size,1,8192)||
           !bound(c.dot_size,0,c.indicator_size)||!bound(c.label_gap,0,8192)||
           c.indicator_size+(c.text.empty()?0:c.label_gap)>8192||!bound(c.font_size,1,256)||
           !bound(c.line_height,0,8192)||(c.font_weight!=400&&c.font_weight!=500&&c.font_weight!=600&&c.font_weight!=700)||
           !valid_motion(c.motion))return false;
        const auto& p=c.palette;
        return color(p.on)&&color(p.off)&&color(p.disabled_on)&&color(p.disabled_off)&&color(p.dot)&&
               color(p.disabled_dot)&&color(p.text)&&color(p.disabled_text)&&color(p.focus);
    }
    bool set_configuration(RadioButtonConfiguration value,TimePoint now){
        if(!validate(value))return false;
        auto next=transition_;const auto target=value.checked?1.0:0.0;
        if(!initialized_){if(!next.reset({target}))return false;}
        else if(!next.retarget({target},value.motion,value.enabled&&value.motion_enabled,now))return false;
        configuration_=std::move(value);transition_=std::move(next);initialized_=true;return true;
    }
    bool set_checked(bool value,TimePoint now)noexcept{
        auto next=transition_;if(!next.retarget({value?1.0:0.0},configuration_.motion,
            configuration_.enabled&&configuration_.motion_enabled,now))return false;
        configuration_.checked=value;transition_=std::move(next);return true;
    }
    [[nodiscard]] std::optional<bool> activate(TimePoint now)noexcept{
        if(!configuration_.enabled||configuration_.checked||!set_checked(true,now))return std::nullopt;return true;
    }
    [[nodiscard]] bool advance(TimePoint now)noexcept{return transition_.advance(now);}
    void finish_animation()noexcept{transition_.settle();}
    [[nodiscard]] RadioButtonVisual visual(RadioButtonInteraction i)const noexcept{
        const auto& c=configuration_;const auto& p=c.palette;const bool enabled=c.enabled&&i.enabled;
        return {enabled?(c.checked?p.on:p.off):(c.checked?p.disabled_on:p.disabled_off),enabled?p.dot:p.disabled_dot,
                enabled?p.text:p.disabled_text,p.focus,dot_scale()>0&&c.dot_size>0,enabled&&i.focused};
    }
    [[nodiscard]] std::optional<RadioButtonGeometry> geometry(double width,double height,double text_width,double text_height)const noexcept{
        const auto extent=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        const auto metric=[](double n){return std::isfinite(n)&&n>=0;};
        if(!extent(width)||!extent(height)||!metric(text_width)||!metric(text_height))return std::nullopt;
        RadioButtonGeometry result;const auto size=std::min({configuration_.indicator_size,width,height});
        result.indicator={0,(height-size)/2,size,size};result.indicator_radius=size/2;result.draw_indicator=size>0;
        const auto dot=std::min(size,configuration_.dot_size*dot_scale());
        result.dot={(size-dot)/2,(height-dot)/2,dot,dot};result.dot_radius=dot/2;result.draw_dot=dot>0;
        if(!configuration_.text.empty()&&text_width>0&&text_height>0){
            const auto x=std::min(width,size+configuration_.label_gap);
            const auto w=std::min(text_width,std::max(0.0,width-x)),h=std::min(height,text_height);
            result.text={x,(height-h)/2,w,h};result.draw_text=w>0&&h>0;
        }
        return result;
    }
private:
    RadioButtonConfiguration configuration_;
    MotionTransition<1> transition_{{0}};
    bool initialized_=false;
};
} // namespace lvui
