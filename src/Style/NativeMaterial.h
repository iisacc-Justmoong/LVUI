#pragma once
#include <LVUI/Material.h>
#include <LVUI/NativeMaterialGradient.h>
#include <utility>
namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor material_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
inline MaterialDensity material_density(MaterialDensity value)noexcept{return value;}
inline MaterialBackdrop material_backdrop(MaterialBackdrop value)noexcept{return value;}
template<class Widget>struct MaterialSourcesValue final {
    Widget background,content;friend bool operator==(const MaterialSourcesValue&,const MaterialSourcesValue&)=default;
};
template<class NativeModel,class NativeGradient>struct MaterialHostPolicy final {
    using Model=NativeModel;using Gradient=NativeGradient;using Options=MaterialConfiguration;using Color=StyleColor;using Backdrop=MaterialBackdrop;
};
template<template<class>class NativeModel,template<class>class NativeGradient,template<class>class NativeHost,class Widget>
using MaterialBinding=NativeHost<MaterialHostPolicy<NativeModel<MaterialModelPolicy>,
    MaterialGradientBinding<NativeGradient,NativeModel<MaterialModelPolicy>>>>;
} // namespace lvui::native
