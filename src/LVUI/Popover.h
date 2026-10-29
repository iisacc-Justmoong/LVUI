#pragma once
#include "StyleTokens.h"
#include "Text.h"
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
struct PopoverSize final {double width=0,height=0;friend bool operator==(const PopoverSize&,const PopoverSize&)=default;};
struct PopoverConfiguration final {
    double width=0,height=0,padding=12,margins=8,corner_radius=12,surface_opacity=.25;
    std::optional<StyleColor> surface_color;
    bool modal=false,dim=false,focus_on_open=true,clip_content=false,dismiss_on_escape=true,dismiss_on_outside_press=true;
    std::optional<bool> dismiss_on_context_request;
    std::string accessibility_name="Popover";
    friend bool operator==(const PopoverConfiguration&,const PopoverConfiguration&)=default;
};

// Platform input and retained surfaces are injected by the application. This
// value model owns the design parameters and validates candidate updates.
class Popover final {
    PopoverConfiguration configuration_;
    [[nodiscard]] static bool scalar(double value,double limit=8192)noexcept{return std::isfinite(value)&&value>=0&&value<=limit;}
public:
    [[nodiscard]] static bool validate(const PopoverConfiguration& value)noexcept{
        const auto color=[](StyleColor c){return scalar(c.red,1)&&scalar(c.green,1)&&scalar(c.blue,1)&&scalar(c.alpha,1);};
        return scalar(value.width)&&scalar(value.height)&&scalar(value.padding)&&scalar(value.margins)&&scalar(value.corner_radius)&&
            scalar(value.surface_opacity,1)&&(!value.surface_color||color(*value.surface_color))&&valid_accessibility_text(value.accessibility_name);
    }
    [[nodiscard]] const PopoverConfiguration& configuration()const noexcept{return configuration_;}
    bool set_configuration(PopoverConfiguration value){if(!validate(value))return false;configuration_=std::move(value);return true;}
    [[nodiscard]] std::optional<PopoverSize> natural_size(PopoverSize content)const noexcept{
        if(!scalar(content.width)||!scalar(content.height))return std::nullopt;
        const auto width=configuration_.width?configuration_.width:content.width+2*configuration_.padding;
        const auto height=configuration_.height?configuration_.height:content.height+2*configuration_.padding;
        if(!scalar(width)||!scalar(height))return std::nullopt;return PopoverSize{width,height};
    }
};
} // namespace lvui
