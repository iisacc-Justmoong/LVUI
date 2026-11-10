#pragma once
#include <LVUI/Modal.h>
#include <LVUI/NativeAlertButton.h>
#include <LVUI/NativeButton.h>
#include <LVUI/NativeIconCache.h>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor modal_color(const Color& value) noexcept {
    return {value.red,value.green,value.blue,value.alpha};
}
template<class Image>struct ModalInputValue final {
    ModalConfiguration configuration;
    std::shared_ptr<const Image> icon;
    std::shared_ptr<const IconSource> icon_source;
    std::int64_t icon_revision=0;
    friend bool operator==(const ModalInputValue&,const ModalInputValue&)=default;
};
struct NativeModalCallbacks final {
    std::array<std::function<void()>,3> actions;
    std::function<void()> opened,closed,canceled;
};
template<class Builder,class Widget>
using ModalContentFactoryValue=std::function<ii::core::Result<void>(Builder&,Widget)>;
struct ModalHostModelPolicy final {
    using Configuration=ModalConfiguration;using Shape=ModalShape;using Rect=ModalRect;using Action=ModalAction;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
    [[nodiscard]] static bool valid_color(StyleColor value)noexcept{return valid_color_value(value);}
    [[nodiscard]] static bool validate_extra(const Configuration&)noexcept{return true;}
    [[nodiscard]] static std::string_view fallback_action(const Configuration&,unsigned index)noexcept{
        constexpr std::array<std::string_view,3> values{"OK","Cancel","More"};return values[index];
    }
};
template<class NativeModel,class NativeButton,class NativeAlertButtonModel,class Decoder,class Widget,class Builder,class Presentation>
struct ModalHostPolicy final {
    using Model=NativeModel;using Input=ModalInputValue<typename Decoder::Image>;
    using Configuration=ModalConfiguration;using Shape=ModalShape;using Color=StyleColor;
    using IconCache=NativeIconCache<Decoder>;using Button=NativeButton;using ButtonInput=ButtonInputValue<typename Decoder::Image>;
    using Callbacks=NativeModalCallbacks;using ContentFactory=ModalContentFactoryValue<Builder,Widget>;
    [[nodiscard]] static bool valid_presentation(const Input&)noexcept{return true;}
    [[nodiscard]] static double text_spacing(const Input&)noexcept{return 8;}
    [[nodiscard]] static double icon_padding(const Input&)noexcept{return 8;}
    [[nodiscard]] static double icon_border_width(const Input&)noexcept{return 1;}
    [[nodiscard]] static std::string_view content_root_id()noexcept{return "@lvui-modal-content";}
    [[nodiscard]] static auto title_options(const Input&,const Configuration& config){
        return Presentation::title(config.title,StyleColor{1,1,1,.9});
    }
    [[nodiscard]] static auto description_options(const Input&,const Model& model){
        return Presentation::description(std::string(model.description()),StyleColor{1,1,1,.8});
    }
    [[nodiscard]] static ii::core::Result<ButtonInput> action_input(const Input&,unsigned index,const ModalAction& action){
        return Presentation::template action<ButtonInput,AlertButtonConfiguration,NativeAlertButtonModel>(
            index,action,ButtonTone::primary,ButtonTone::normal);
    }
};
template<template<class>class NativeButtonModel,template<class>class NativeButtonHost,
         template<class>class NativeAlertButtonModel,template<class>class NativeModel,template<class>class NativeHost,
         class Decoder,class Widget,class Builder,class Presentation>
using ModalBinding=NativeHost<ModalHostPolicy<NativeModel<ModalHostModelPolicy>,
    ButtonBinding<NativeButtonModel,NativeButtonHost,Decoder,Widget>,
    NativeAlertButtonModel<AlertButtonModelPolicy<NativeButtonModel<ButtonHostModelPolicy>>>,
    Decoder,Widget,Builder,Presentation>>;
} // namespace lvui::native
