#pragma once
#include "Button.h"
#include "Navigation.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace lvui {
struct LinkEvent final {NavigationRequest request;};
using LinkMethod=std::function<void(const LinkEvent&)>;
struct LinkPalette final {
    StyleColor link{.0812049285,.5099760294,1,1};
    StyleColor hover{1,1,1,.9};
    std::optional<StyleColor> pressed;
    StyleColor disabled{1,1,1,.6};
    StyleColor focus{.0812049285,.5099760294,1,1};
};
struct LinkConfiguration final {
    std::string text,accessibility_name,href="/";
    ii::core::Value params{ii::core::Value::Object{}};
    std::weak_ptr<NavigationTarget> router,navigator;
    std::shared_ptr<const NavigationPage> target;
    bool replace=false,enabled=true,underline=false,show_focus_ring=true;
    ButtonTone tone=ButtonTone::borderless;
    double horizontal_padding=14,vertical_padding=10,minimum_height=36,corner_radius=8;
    std::optional<double> width,height;
    double font_size=12,line_height=12;
    unsigned font_weight=600;
    LinkPalette palette;
};
struct LinkInteraction final {bool enabled=true,hovered=false,pressed=false,focused=false;};
struct LinkVisual final {bool enabled=false,focused=false,underline=false;StyleColor text,focus;};
struct LinkRect final {double x=0,y=0,width=0,height=0;};
struct LinkGeometry final {LinkRect text;double radius=0;bool draw_text=false;};

// Layout, appearance, target priority and reentry are independent of the host.
class Link final {
public:
    [[nodiscard]] const LinkConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] static bool validate(const LinkConfiguration& c)noexcept {
        const auto range=[](double v,double low,double high){return std::isfinite(v)&&v>=low&&v<=high;};
        const auto color=[&](StyleColor v){return range(v.red,0,1)&&range(v.green,0,1)&&range(v.blue,0,1)&&range(v.alpha,0,1);};
        const auto optional=[&](std::optional<double> v){return !v||range(*v,0,8192);};
        return valid_accessibility_text(c.text)&&valid_accessibility_text(c.accessibility_name)&&valid_accessibility_text(c.href)&&
            valid_navigation_parameters(c.params)&&unsigned(c.tone)<=unsigned(ButtonTone::disabled)&&
            range(c.horizontal_padding,0,8192)&&range(c.vertical_padding,0,8192)&&
            c.horizontal_padding*2<=8192&&c.vertical_padding*2<=8192&&range(c.minimum_height,0,8192)&&
            range(c.corner_radius,0,8192)&&optional(c.width)&&optional(c.height)&&
            range(c.font_size,1,256)&&range(c.line_height,0,8192)&&
            (c.font_weight==400||c.font_weight==500||c.font_weight==600||c.font_weight==700)&&
            color(c.palette.link)&&color(c.palette.hover)&&(!c.palette.pressed||color(*c.palette.pressed))&&
            color(c.palette.disabled)&&color(c.palette.focus);
    }
    bool set_configuration(LinkConfiguration value){if(!validate(value))return false;configuration_=std::move(value);return true;}
    [[nodiscard]] LinkVisual visual(LinkInteraction interaction)const noexcept {
        const auto& c=configuration_;const bool enabled=c.enabled&&interaction.enabled&&c.tone!=ButtonTone::disabled;
        const auto& p=c.palette;
        const auto pressed=p.pressed.value_or(StyleColor{p.link.red/1.2,p.link.green/1.2,p.link.blue/1.2,p.link.alpha});
        return {enabled,enabled&&interaction.focused&&c.show_focus_ring,c.underline,
            !enabled?p.disabled:interaction.pressed?pressed:interaction.hovered?p.hover:p.link,p.focus};
    }
    [[nodiscard]] std::optional<LinkGeometry> geometry(double width,double height,double text_width,double text_height,bool has_children)const noexcept {
        const auto extent=[](double v){return std::isfinite(v)&&v>=0&&v<=8192;};
        const auto metric=[](double v){return std::isfinite(v)&&v>=0;};
        if(!extent(width)||!extent(height)||!metric(text_width)||!metric(text_height))return std::nullopt;
        LinkGeometry result;result.radius=std::min({configuration_.corner_radius,width/2,height/2});
        if(has_children||configuration_.text.empty())return result;
        const auto x=std::min(configuration_.horizontal_padding,width/2);
        const auto w=std::min(text_width,std::max(0.,width-2*x));
        const auto h=std::min(text_height,std::max(0.,height-2*std::min(configuration_.vertical_padding,height/2)));
        result.text={x,(height-h)/2,w,h};result.draw_text=w>0&&h>0;return result;
    }
    // An empty result means no destination/target was available; false means a
    // target explicitly declined. A nested request on the same Link is busy.
    [[nodiscard]] ii::core::Result<std::optional<bool>> navigate(std::shared_ptr<NavigationTarget> fallback={}) {
        if(navigating_)return ii::core::fail(ii::core::ErrorCode::busy,"Link navigation is already active");
        if(configuration_.href.empty()&&!configuration_.target)return std::optional<bool>{};
        auto target=configuration_.router.lock();if(!target)target=configuration_.navigator.lock();if(!target)target=std::move(fallback);
        if(!target)return std::optional<bool>{};
        NavigationRequest request{configuration_.href,configuration_.params,configuration_.target,configuration_.replace};
        struct Guard final {bool& active;explicit Guard(bool& value):active(value){active=true;}~Guard(){active=false;}}guard(navigating_);
        try {auto result=target->navigate(request);if(!result)return std::unexpected(result.error());return std::optional<bool>{*result};}
        catch(...){return ii::core::fail(ii::core::ErrorCode::failed,"Link navigation target threw",std::current_exception());}
    }
private:
    LinkConfiguration configuration_;
    bool navigating_=false;
};
} // namespace lvui
