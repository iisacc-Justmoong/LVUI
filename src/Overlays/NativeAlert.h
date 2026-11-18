#pragma once
#include <LVUI/Alert.h>
#include <LVUI/NativeAlertButton.h>
#include <LVUI/NativeIconCache.h>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace lvui::native {
struct AlertModelPolicy final {
    using Configuration=AlertConfiguration;using Shape=AlertShape;using Rect=AlertRect;
    using Action=AlertAction;using Layout=AlertLayout;using Tone=ButtonTone;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
    [[nodiscard]] static bool valid_color(StyleColor value)noexcept{return valid_color_value(value);}
};
struct AlertHostStyle final {
    std::optional<AlertTypography> title_typography, description_typography;
    std::optional<StyleColor> icon_background, icon_border, action_text, action_border;
    friend bool operator==(const AlertHostStyle&,const AlertHostStyle&)=default;
};
template<class Image>struct AlertInputValue final {
    AlertConfiguration configuration;
    std::shared_ptr<const Image> icon;
    std::string icon_name;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision=0;
    bool use_default_icon=true;
    AlertHostStyle style;
    friend bool operator==(const AlertInputValue&,const AlertInputValue&)=default;
};
template<class Widget>struct NativeAlertCallbacksValue final {
    using Event=ButtonEventValue<Widget>;
    std::array<std::function<void(const Event&)>,3> methods;
    std::array<std::function<void()>,3> clicked;
    std::function<void()> opened,closed,canceled;
};
template<class NativeModel,class NativeAlertButtonModel,class NativeAlertButton,class Decoder,class Widget,class Presentation>
struct AlertHostPolicy final {
    using Model=NativeModel;using Input=AlertInputValue<typename Decoder::Image>;using Configuration=AlertConfiguration;
    using Shape=AlertShape;using Rect=AlertRect;using Color=StyleColor;
    using IconCache=NativeIconCache<Decoder>;using Button=NativeAlertButton;
    using ButtonConfiguration=AlertButtonConfiguration;using ButtonCallbacks=NativeAlertButtonCallbacksValue<Widget>;
    using ButtonEvent=ButtonEventValue<Widget>;using Callbacks=NativeAlertCallbacksValue<Widget>;
    [[nodiscard]] static std::uint64_t source_revision(const Input& input)noexcept {
        return input.configuration.show_icon&&!input.icon&&input.icon_source?input.icon_source->revision():0;
    }
    [[nodiscard]] static bool valid_input(const Input& input)noexcept {
        const auto color=[](const auto& value){return !value||valid_color_value(*value);};
        return valid_accessibility_text(input.icon_name)&&input.icon_revision>=0&&
            !(input.icon&&!input.icon_name.empty())&&(!input.icon_source||!input.icon_name.empty())&&
            valid_alert_typography(input.style.title_typography)&&valid_alert_typography(input.style.description_typography)&&
            color(input.style.icon_background)&&color(input.style.icon_border)&&color(input.style.action_text)&&color(input.style.action_border);
    }
    [[nodiscard]] static ii::core::Result<typename Presentation::ImageInput> prepare_icon(const Input& input,const Model& model,IconCache& icons) {
        auto image=input.icon;const auto& c=model.configuration();const auto count=model.action_count();
        if(c.show_icon&&!image&&(!input.icon_name.empty()||input.use_default_icon)){
            const auto name=input.icon_name.empty()?(count==3?"alertFileText":"alertAdjustments"):input.icon_name;
            auto resolved=icons.resolve(name,input.icon_source,c.icon_extent);
            if(!resolved)return std::unexpected(resolved.error());image=*resolved;
        }
        return Presentation::icon(std::move(image),c.icon_extent,count,
            input.style.icon_background.value_or(figma_defaults::panel_surface_interactive),
            input.style.icon_border.value_or(StyleColor{208./255,215./255,226./255,.18}));
    }
    [[nodiscard]] static ii::core::Result<ButtonConfiguration> action(const Input& input,const Model& model,unsigned index) {
        auto result=Presentation::template action<ButtonConfiguration,NativeAlertButtonModel>(model,index);
        if(!result)return std::unexpected(result.error());
        if(input.style.action_text)result->dialog_text=*input.style.action_text;
        if(input.style.action_border)result->dialog_border=*input.style.action_border;
        return result;
    }
    [[nodiscard]] static auto title_options(const Input& input,const Configuration& c) {
        auto result=Presentation::title(c.title,c.palette.title);
        if(const auto& style=input.style.title_typography){result.font_size=style->font_size;result.line_height=style->line_height;result.font_weight=style->font_weight;}
        return result;
    }
    [[nodiscard]] static auto description_options(const Input& input,const Model& model) {
        auto result=Presentation::description(std::string(model.description()),model.configuration().palette.description);
        if(const auto& style=input.style.description_typography){result.font_size=style->font_size;result.line_height=style->line_height;result.font_weight=style->font_weight;}
        return result;
    }
};
template<template<class>class NativeButtonModel,template<class>class NativeButtonHost,
         template<class>class NativeAlertButtonModel,template<class>class NativeAlertButtonHost,
         template<class>class NativeModel,template<class>class NativeHost,class Decoder,class Widget,class Presentation>
using AlertBinding=NativeHost<AlertHostPolicy<NativeModel<AlertModelPolicy>,
    NativeAlertButtonModel<AlertButtonModelPolicy<NativeButtonModel<ButtonHostModelPolicy>>>,
    AlertButtonBinding<NativeButtonModel,NativeButtonHost,NativeAlertButtonModel,NativeAlertButtonHost,Decoder,Widget>,
    Decoder,Widget,Presentation>>;
} // namespace lvui::native
