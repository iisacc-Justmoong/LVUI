#pragma once
#include <LVUI/NativeMaterial.h>
#include <LVUI/Text.h>
#include <LVUI/TooltipController.h>
#include <LVUI/TooltipOutline.h>
#include <memory>
#include <string>
#include <string_view>
namespace lvui::native {
template<class Motion>
struct TooltipPresentationStyleValue final {
    MaterialConfiguration material=[] {MaterialConfiguration value;value.kind=MaterialKind::panel;
        value.backdrop=MaterialBackdrop::content;value.motion_enabled=false;return value;}();
    double content_padding=12;
    bool scroll_content=true;
    bool motion_enabled=true;
    std::shared_ptr<Motion> motion;
    std::string accessibility_name;
    friend bool operator==(const TooltipPresentationStyleValue&,const TooltipPresentationStyleValue&)=default;
};
template<class Motion,class NativeMaterialModel,class NativeMaterial,class NativeOutline>
struct TooltipPresentationPolicy final {
    using Port=TooltipPresentationPort;using Style=TooltipPresentationStyleValue<Motion>;
    using MaterialModel=NativeMaterialModel;using Material=NativeMaterial;using Outline=NativeOutline;using Layout=TooltipLayout;
    [[nodiscard]] static bool valid_text(std::string_view value)noexcept{return valid_accessibility_text(value);}
};
template<template<class>class Host,class Motion,class MaterialModel,class Material,class Outline>
using TooltipPresentationBinding=Host<TooltipPresentationPolicy<Motion,MaterialModel,Material,Outline>>;
} // namespace lvui::native
