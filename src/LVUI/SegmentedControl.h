#pragma once
#include "StyleTokens.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
namespace lvui {
enum class SegmentedShape { round_rect, cylinder };
struct SegmentedControlConfiguration final {
    double horizontal_padding=4,vertical_padding=3.5,spacing=2,border_width=2,corner_radius=12;
    SegmentedShape shape=SegmentedShape::round_rect;
    bool enabled=true,force_borderless_tone=true;
    StyleColor background{.1370945126,.1402995437,.1419020444,1};
    StyleColor border{.1935568452,.1978072077,.1999323964,1};
    friend bool operator==(const SegmentedControlConfiguration&,const SegmentedControlConfiguration&)=default;
};
struct SegmentedControlGeometry final {double width=0,height=0,radius=0,border_width=0;};
// A presentation container only: children own their actions and selection.
class SegmentedControl final {
    SegmentedControlConfiguration configuration_;
public:
    [[nodiscard]] static bool validate(const SegmentedControlConfiguration& v)noexcept{
        const auto bound=[](double n,double hi){return std::isfinite(n)&&n>=0&&n<=hi;};
        const auto color=[&](StyleColor c){return bound(c.red,1)&&bound(c.green,1)&&bound(c.blue,1)&&bound(c.alpha,1);};
        return bound(v.horizontal_padding,8192)&&bound(v.vertical_padding,8192)&&bound(v.spacing,32767)&&
            bound(v.border_width,8192)&&bound(v.corner_radius,8192)&&
            (v.shape==SegmentedShape::round_rect||v.shape==SegmentedShape::cylinder)&&color(v.background)&&color(v.border);
    }
    [[nodiscard]] bool set_configuration(SegmentedControlConfiguration value){
        if(!validate(value))return false;configuration_=std::move(value);return true;
    }
    [[nodiscard]] const SegmentedControlConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] std::optional<SegmentedControlGeometry> geometry(double width,double height)const noexcept{
        if(!std::isfinite(width)||!std::isfinite(height)||width<0||height<0||width>8192||height>8192)return std::nullopt;
        const auto max_radius=std::min(width,height)/2;
        const auto radius=configuration_.shape==SegmentedShape::cylinder?max_radius:std::min(configuration_.corner_radius,max_radius);
        return SegmentedControlGeometry{width,height,radius,std::min(configuration_.border_width,max_radius)};
    }
};
} // namespace lvui
