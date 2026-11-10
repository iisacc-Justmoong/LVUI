#pragma once
#include <LVUI/Popover.h>
#include <LVUI/Material.h>
#include <LVUI/NativeMaterialGradient.h>
#include <functional>
namespace lvui::native {
template<class Color>[[nodiscard]] inline StyleColor popover_color(const Color& value)noexcept{return {value.red,value.green,value.blue,value.alpha};}
struct NativePopoverCallbacks final {std::function<void()> opened,closed,canceled;};
template<class NativeModel,class NativeMaterial,class NativeGradient>struct PopoverHostPolicy final {
    using Model=NativeModel;using Options=PopoverConfiguration;using Placement=PopoverPlacement;
    using MaterialModel=NativeMaterial;using MaterialOptions=MaterialConfiguration;using MaterialKind=lvui::MaterialKind;
    using Gradient=NativeGradient;using Callbacks=NativePopoverCallbacks;
    [[nodiscard]] static StyleColor default_surface()noexcept{return figma_defaults::panel_surface_solid;}
};
template<template<class>class NativeModel,template<class>class NativeMaterial,template<class>class NativeGradient,template<class>class NativeHost>
using PopoverBinding=NativeHost<PopoverHostPolicy<NativeModel<PopoverModelPolicy>,NativeMaterial<MaterialModelPolicy>,
    MaterialGradientBinding<NativeGradient,NativeMaterial<MaterialModelPolicy>>>>;
} // namespace lvui::native
