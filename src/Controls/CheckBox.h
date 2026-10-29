#pragma once
#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
struct CheckBoxPalette final {
    StyleColor checked{.0812049285,.5099760294,1,1},unchecked{1,1,1,.8};
    StyleColor disabled_checked{.1935568452,.1978072077,.1999323964,1};
    StyleColor disabled_unchecked{.1935568452,.1978072077,.1999323964,1};
    StyleColor mark{1,1,1,.8},disabled_mark{1,1,1,.3};
    StyleColor checked_border{0,0,0,0},unchecked_border{1,1,1,.8};
    StyleColor disabled_checked_border{.1935568452,.1978072077,.1999323964,1};
    StyleColor disabled_unchecked_border{0,0,0,0};
    StyleColor text{1,1,1,.8},disabled_text{1,1,1,.3},focus{.0812049285,.5099760294,1,1};
    friend bool operator==(const CheckBoxPalette&,const CheckBoxPalette&) = default;
};
struct CheckBoxConfiguration final {
    std::string text,accessibility_name;
    bool enabled=true,checked=false;
    double box_size=17,frame_padding=.5,box_radius=3.5,mark_stroke_width=2,label_gap=6;
    double checked_border_width=0,unchecked_border_width=.5;
    double disabled_checked_border_width=.5,disabled_unchecked_border_width=0;
    double font_size=13,line_height=13;
    unsigned font_weight=500;
    CheckBoxPalette palette;
    friend bool operator==(const CheckBoxConfiguration&,const CheckBoxConfiguration&) = default;
};
struct CheckBoxInteraction final { bool enabled=true,hovered=false,pressed=false,focused=false; };
struct CheckBoxVisual final {
    StyleColor fill,mark,border,text,focus;
    double border_width=0;
    bool draw_mark=false,focused=false;
};
struct CheckBoxRect final { double x=0,y=0,width=0,height=0; };
struct CheckBoxGeometry final {
    CheckBoxRect box,text;
    double radius=0;
    bool draw_box=false,draw_text=false;
};

// The state and geometry are independent of the native input/rendering host.
class CheckBox final {
public:
    [[nodiscard]] const CheckBoxConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] bool checked()const noexcept{return configuration_.checked;}
    [[nodiscard]] static bool validate(const CheckBoxConfiguration& c)noexcept{
        const auto bound=[](double v,double low,double high){return std::isfinite(v)&&v>=low&&v<=high;};
        const auto color=[&](StyleColor v){return bound(v.red,0,1)&&bound(v.green,0,1)&&bound(v.blue,0,1)&&bound(v.alpha,0,1);};
        if(!valid_accessibility_text(c.text)||!valid_accessibility_text(c.accessibility_name)||
           !bound(c.box_size,1,8192)||!bound(c.frame_padding,0,8192)||!bound(c.box_radius,0,8192)||
           !bound(c.mark_stroke_width,0,c.box_size)||!bound(c.label_gap,0,8192)||
           !bound(c.checked_border_width,0,c.box_size/2)||!bound(c.unchecked_border_width,0,c.box_size/2)||
           !bound(c.disabled_checked_border_width,0,c.box_size/2)||!bound(c.disabled_unchecked_border_width,0,c.box_size/2)||
           c.box_size+2*c.frame_padding>8192||c.box_size+2*c.frame_padding+(c.text.empty()?0:c.label_gap)>8192||
           !bound(c.font_size,1,256)||!bound(c.line_height,0,8192)||
           (c.font_weight!=400&&c.font_weight!=500&&c.font_weight!=600&&c.font_weight!=700))return false;
        const auto& p=c.palette;
        return color(p.checked)&&color(p.unchecked)&&color(p.disabled_checked)&&color(p.disabled_unchecked)&&
               color(p.mark)&&color(p.disabled_mark)&&color(p.checked_border)&&color(p.unchecked_border)&&
               color(p.disabled_checked_border)&&color(p.disabled_unchecked_border)&&color(p.text)&&color(p.disabled_text)&&color(p.focus);
    }
    bool set_configuration(CheckBoxConfiguration value){
        if(!validate(value))return false;configuration_=std::move(value);return true;
    }
    [[nodiscard]] std::optional<bool> activate()noexcept{
        if(!configuration_.enabled)return std::nullopt;
        configuration_.checked=!configuration_.checked;return configuration_.checked;
    }
    [[nodiscard]] CheckBoxVisual visual(CheckBoxInteraction i)const noexcept{
        const auto& c=configuration_;const auto& p=c.palette;const bool enabled=c.enabled&&i.enabled;
        auto fill=enabled?(c.checked?p.checked:p.unchecked):(c.checked?p.disabled_checked:p.disabled_unchecked);
        if(enabled&&(i.pressed||i.hovered)){
            const auto divisor=c.checked?(i.pressed?1.2:1.08):(i.pressed?1.18:1.06);
            fill.red/=divisor;fill.green/=divisor;fill.blue/=divisor;
        }
        return {fill,enabled?p.mark:p.disabled_mark,
                enabled?(c.checked?p.checked_border:p.unchecked_border):(c.checked?p.disabled_checked_border:p.disabled_unchecked_border),
                enabled?p.text:p.disabled_text,p.focus,
                enabled?(c.checked?c.checked_border_width:c.unchecked_border_width):(c.checked?c.disabled_checked_border_width:c.disabled_unchecked_border_width),
                c.checked,enabled&&i.focused};
    }
    [[nodiscard]] std::optional<CheckBoxGeometry> geometry(double width,double height,double text_width,double text_height)const noexcept{
        const auto extent=[](double n){return std::isfinite(n)&&n>=0&&n<=8192;};
        const auto metric=[](double n){return std::isfinite(n)&&n>=0;};
        if(!extent(width)||!extent(height)||!metric(text_width)||!metric(text_height))return std::nullopt;
        const auto padding=std::min(configuration_.frame_padding,width/2);
        const auto size=std::min({configuration_.box_size,std::max(0.0,width-2*padding),std::max(0.0,height-2*configuration_.frame_padding)});
        CheckBoxGeometry result;result.box={padding,(height-size)/2,size,size};
        result.radius=std::min(configuration_.box_radius,size/2);result.draw_box=size>0;
        if(!configuration_.text.empty()&&text_width>0&&text_height>0){
            const auto x=std::min(width,padding+size+configuration_.label_gap);
            const auto w=std::min(text_width,std::max(0.0,width-x-padding)),h=std::min(height,text_height);
            result.text={x,(height-h)/2,w,h};result.draw_text=w>0&&h>0;
        }
        return result;
    }
    // A rounded two-segment mark in normalized box coordinates. Sampling this
    // function produces an LVUI-owned mask independent of any font or asset.
    [[nodiscard]] double mark_coverage(double x,double y)const noexcept{
        if(!std::isfinite(x)||!std::isfinite(y)||configuration_.mark_stroke_width<=0)return 0;
        const auto distance=[&](double ax,double ay,double bx,double by){
            const auto dx=bx-ax,dy=by-ay;
            const auto t=std::clamp(((x-ax)*dx+(y-ay)*dy)/(dx*dx+dy*dy),0.0,1.0);
            return std::hypot(x-(ax+t*dx),y-(ay+t*dy));
        };
        const auto radius=configuration_.mark_stroke_width/(2*configuration_.box_size);
        const auto signed_distance=radius-std::min(distance(.27,.49,.43,.67),distance(.43,.67,.73,.29));
        return std::clamp(signed_distance*272+.5,0.0,1.0);
    }
private:
    CheckBoxConfiguration configuration_;
};
} // namespace lvui
