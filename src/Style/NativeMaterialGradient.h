#pragma once
#include <LVUI/Material.h>
#include <cstdint>

namespace lvui::native {
// Authored material values and radial edge colors are supplied by the app.
template<class NativeMaterialModel>struct MaterialGradientPolicy final {
    using Material=NativeMaterialModel;
    inline static constexpr auto edge_rgb=material_radial_defaults::edge_rgb;
    [[nodiscard]] static constexpr std::uint64_t geometry_revision(const Material&)noexcept{return 0;}
};
template<template<class>class NativeGradient,class NativeMaterialModel>
using MaterialGradientBinding=NativeGradient<MaterialGradientPolicy<NativeMaterialModel>>;
} // namespace lvui::native
