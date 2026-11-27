#pragma once
#include <LVUI/Material.h>
#include <algorithm>
#include <array>
#include <cstdint>

#define LVUI_MATERIAL_DYNAMIC_EDGE_API 1

namespace lvui::native {
// Authored material values and radial edge colors are supplied by the app.
template<class NativeMaterialModel>struct MaterialGradientPolicy final {
    using Material=NativeMaterialModel;
    inline static constexpr auto edge_rgb=material_radial_defaults::edge_rgb;
    // Apply the authored blue material's saturation/value increase to the
    // current accent while preserving hue and staying within the sRGB gamut.
    [[nodiscard]] static std::array<double,3> edge_color(const Material& material)noexcept {
        const auto accent=material.displayed_primary();
        const std::array<double,3> color{accent.red,accent.green,accent.blue};
        const auto high=(std::max)({color[0],color[1],color[2]});
        const auto low=(std::min)({color[0],color[1],color[2]});
        if(high<=0)return {0,0,0};
        const auto baseline=lvui::figma_defaults::accent_blue_default;
        const auto baselineValue=(std::max)({baseline.red,baseline.green,baseline.blue});
        const auto edgeValue=(std::max)({edge_rgb[0],edge_rgb[1],edge_rgb[2]});
        const auto gain=edgeValue/baselineValue;
        const auto value=(std::min)(1.,high*gain);
        const auto saturation=(std::min)(1.,(high-low)/high*gain);
        const auto chroma=high-low;
        std::array<double,3> result{};
        for(std::size_t i=0;i<result.size();++i) {
            const auto position=chroma>0?(color[i]-low)/chroma:0.;
            result[i]=value*(1.-saturation+saturation*position);
        }
        return result;
    }
    [[nodiscard]] static constexpr std::uint64_t geometry_revision(const Material&)noexcept{return 0;}
};
template<template<class>class NativeGradient,class NativeMaterialModel>
using MaterialGradientBinding=NativeGradient<MaterialGradientPolicy<NativeMaterialModel>>;
} // namespace lvui::native
