#pragma once
#include "StyleTokens.h"
#include "Text.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
enum class AppCardShape { round_rect,cylinder };
struct AppCardConfiguration final {
    std::string title,subtitle;
    bool enabled=true;
    double width=0,height=0,padding=18,spacing=10,corner_radius=12;
    AppCardShape shape=AppCardShape::round_rect;
    StyleColor background{.1139791608,.1166428775,.1166428775,1};
    StyleColor title_color{1,1,1,.9},subtitle_color{1,1,1,.8},divider_color{1,1,1,.12};
    friend bool operator==(const AppCardConfiguration&,const AppCardConfiguration&)=default;
};
struct AppCardSize final {double width=0,height=0;friend bool operator==(const AppCardSize&,const AppCardSize&)=default;};
struct AppCardRect final {double x=0,y=0,width=0,height=0;};
struct AppCardMeasurements final {double title_width=0,subtitle_width=0,content_width=0,content_height=0;};
struct AppCardGeometry final {
    double radius=0;
    bool header=false;
    AppCardRect title,subtitle,content;
    std::optional<AppCardRect> separator;
};

// This model owns the card's style and geometry. A host supplies text and
// content measurements and is free to render or embed the body independently.
class AppCard final {
    AppCardConfiguration configuration_;
    [[nodiscard]] static bool scalar(double value,double limit=8192)noexcept{return std::isfinite(value)&&value>=0&&value<=limit;}
    [[nodiscard]] static bool color(StyleColor value)noexcept {
        return scalar(value.red,1)&&scalar(value.green,1)&&scalar(value.blue,1)&&scalar(value.alpha,1);
    }
    [[nodiscard]] double heading_height()const noexcept {
        const auto title=!configuration_.title.empty(),subtitle=!configuration_.subtitle.empty();
        return (title?17.:0.)+(subtitle?12.:0.)+(title&&subtitle?4.:0.);
    }
public:
    [[nodiscard]] const AppCardConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] static bool validate(const AppCardConfiguration& value)noexcept {
        return valid_accessibility_text(value.title)&&valid_accessibility_text(value.subtitle)&&
            scalar(value.width)&&scalar(value.height)&&scalar(value.padding)&&scalar(value.spacing)&&scalar(value.corner_radius)&&
            (value.shape==AppCardShape::round_rect||value.shape==AppCardShape::cylinder)&&
            color(value.background)&&color(value.title_color)&&color(value.subtitle_color)&&color(value.divider_color);
    }
    bool set_configuration(AppCardConfiguration value){if(!validate(value))return false;configuration_=std::move(value);return true;}
    [[nodiscard]] std::optional<AppCardSize> natural_size(const AppCardMeasurements& measurements)const noexcept {
        constexpr double measurement_limit=1000000000.;
        if(!scalar(measurements.title_width,measurement_limit)||!scalar(measurements.subtitle_width,measurement_limit)||
           !scalar(measurements.content_width,measurement_limit)||!scalar(measurements.content_height,measurement_limit))return std::nullopt;
        const bool header=heading_height()>0;
        const double content_width=std::max({configuration_.title.empty()?0.:measurements.title_width,
            configuration_.subtitle.empty()?0.:measurements.subtitle_width,measurements.content_width});
        const auto width=std::min(8192.,std::max(280.,content_width+2*configuration_.padding));
        const auto height=std::min(8192.,2*configuration_.padding+std::max(1.,measurements.content_height)+
            (header?heading_height()+2*configuration_.spacing+1:0));
        return AppCardSize{configuration_.width?configuration_.width:width,configuration_.height?configuration_.height:height};
    }
    [[nodiscard]] std::optional<AppCardGeometry> geometry(double width,double height)const noexcept {
        if(!scalar(width)||!scalar(height))return std::nullopt;
        AppCardGeometry result;result.header=heading_height()>0;
        result.radius=std::min({configuration_.shape==AppCardShape::cylinder?std::min(width,height)/2:configuration_.corner_radius,width/2,height/2});
        const auto x=std::min(configuration_.padding,width/2),top=std::min(configuration_.padding,height/2);
        const auto inner_width=std::max(0.,width-2*x),bottom=std::max(top,height-top);
        const auto rectangle=[&](double y,double h){const auto start=std::clamp(y,0.,height);return AppCardRect{x,start,inner_width,std::max(0.,std::min(h,height-start))};};
        double cursor=top;
        if(!configuration_.title.empty()){result.title=rectangle(cursor,17);cursor+=17;}
        if(!configuration_.subtitle.empty()){
            if(!configuration_.title.empty())cursor+=4;result.subtitle=rectangle(cursor,12);cursor+=12;
        }
        if(result.header){cursor+=configuration_.spacing;result.separator=rectangle(cursor,1);cursor+=1+configuration_.spacing;}
        const auto body=std::clamp(cursor,0.,height);result.content={x,body,inner_width,std::max(0.,bottom-body)};
        return result;
    }
};
} // namespace lvui
