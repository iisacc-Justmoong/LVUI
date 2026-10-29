#pragma once
#include <LVUI/StyleTokens.h>
#include <LVUI/Text.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace lvui {
struct AppHeaderConfiguration final {
    std::string title,subtitle;
    bool enabled=true,menu_visible=false,compact=false;
    double width=0,height=0;
    std::optional<double> content_horizontal_padding,content_vertical_padding,row_spacing,action_spacing,menu_button_padding;
    StyleColor background{.083941631,.083941631,.086313099,1};
    StyleColor title_color{1,1,1,.9},subtitle_color{1,1,1,.8};
    StyleColor menu_background{.1139791608,.1166428775,.1166428775,1};
    StyleColor menu_pressed_background{.1230252981,.1252803057,.1275353134,1};
    friend bool operator==(const AppHeaderConfiguration&,const AppHeaderConfiguration&)=default;
};
struct AppHeaderMetrics final {
    double horizontal_padding=0,vertical_padding=0,row_spacing=0,action_spacing=0,menu_padding=0,menu_height=0;
};
struct AppHeaderMeasurements final {double title_width=0,subtitle_width=0,actions_width=0,actions_height=0;};
struct AppHeaderSize final {double width=0,height=0;};

// The host measures text and action content. LVUI owns defaults, compact mode,
// validation, and the natural-size policy without a concrete widget dependency.
class AppHeader final {
    AppHeaderConfiguration configuration_;
    [[nodiscard]] static bool scalar(double value,double maximum=8192)noexcept{return std::isfinite(value)&&value>=0&&value<=maximum;}
    [[nodiscard]] static bool optional_scalar(const std::optional<double>& value)noexcept{return !value||scalar(*value);}
    [[nodiscard]] static bool color(StyleColor value)noexcept {
        return scalar(value.red,1)&&scalar(value.green,1)&&scalar(value.blue,1)&&scalar(value.alpha,1);
    }
public:
    [[nodiscard]] const AppHeaderConfiguration& configuration()const noexcept{return configuration_;}
    [[nodiscard]] static bool validate(const AppHeaderConfiguration& value)noexcept {
        return valid_accessibility_text(value.title)&&valid_accessibility_text(value.subtitle)&&scalar(value.width)&&scalar(value.height)&&
            optional_scalar(value.content_horizontal_padding)&&optional_scalar(value.content_vertical_padding)&&optional_scalar(value.row_spacing)&&
            optional_scalar(value.action_spacing)&&(!value.menu_button_padding||scalar(*value.menu_button_padding,4090))&&color(value.background)&&color(value.title_color)&&
            color(value.subtitle_color)&&color(value.menu_background)&&color(value.menu_pressed_background);
    }
    bool set_configuration(AppHeaderConfiguration value){if(!validate(value))return false;configuration_=std::move(value);return true;}
    [[nodiscard]] AppHeaderMetrics resolved()const noexcept {
        const auto compact=configuration_.compact;
        return {configuration_.content_horizontal_padding.value_or(compact?10.:16.),
            configuration_.content_vertical_padding.value_or(compact?10.:16.),configuration_.row_spacing.value_or(compact?8.:12.),
            configuration_.action_spacing.value_or(compact?6.:8.),configuration_.menu_button_padding.value_or(compact?8.:10.),compact?28.:32.};
    }
    [[nodiscard]] std::optional<AppHeaderSize> natural_size(const AppHeaderMeasurements& value)const noexcept {
        constexpr auto limit=1000000000.;
        if(!scalar(value.title_width,limit)||!scalar(value.subtitle_width,limit)||!scalar(value.actions_width,limit)||!scalar(value.actions_height,limit))return std::nullopt;
        const auto metrics=resolved();const bool title=!configuration_.title.empty(),subtitle=!configuration_.subtitle.empty();
        const auto title_width=std::max(title?value.title_width:0.,subtitle?value.subtitle_width:0.);
        const auto title_height=(title?22.:0.)+(subtitle?12.:0.)+(title&&subtitle?4.:0.);
        const auto menu_width=configuration_.menu_visible?2*metrics.menu_padding+12:0.;
        const auto menu_gap=configuration_.menu_visible?metrics.row_spacing:0.;
        const auto action_gap=value.actions_width>0?metrics.row_spacing:0.;
        const auto width=std::min(8192.,2*metrics.horizontal_padding+menu_width+menu_gap+title_width+action_gap+value.actions_width);
        const auto height=std::min(8192.,std::max(56.,32+std::max({title_height,configuration_.menu_visible?metrics.menu_height:0.,value.actions_height})));
        return AppHeaderSize{configuration_.width?configuration_.width:width,configuration_.height?configuration_.height:height};
    }
};
} // namespace lvui
