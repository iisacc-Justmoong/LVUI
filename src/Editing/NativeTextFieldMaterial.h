#pragma once
#include <LVUI/SearchFieldMaterial.generated.h>
#include <LVUI/SearchFieldTokens.generated.h>

namespace lvui::native::text_field_material {
enum class Variant { standard, inline_ };
// Immutable design values supplied to the application's chosen material engine.
struct Policy final {
    using Color=StyleColor;using Variant=text_field_material::Variant;
    inline static constexpr auto& fill_gradient=search_field_material::fill_gradient;
    inline static constexpr auto& disabled_fill_gradient=search_field_material::disabled_fill_gradient;
    inline static constexpr auto& inline_fill_gradient=search_field_material::inline_fill_gradient;
    inline static constexpr auto& stroke_gradient=search_field_material::stroke_gradient;
    inline static constexpr auto& inner_shadows=search_field_material::inner_shadows;
    inline static constexpr auto& inline_inner_shadows=search_field_material::inline_inner_shadows;
    inline static constexpr auto reflection=search_field_defaults::reflection;
    inline static constexpr auto background=search_field_defaults::background;
    inline static constexpr auto disabled_background=search_field_defaults::disabled_background;
    inline static constexpr auto inline_background=search_field_defaults::inline_background;
    inline static constexpr auto inline_disabled_background=search_field_defaults::inline_disabled_background;
    inline static constexpr auto border_width=search_field_defaults::border_width;
    inline static constexpr auto radius=search_field_defaults::radius;
    inline static constexpr auto blur_radius=search_field_material::blur_radius;
    inline static constexpr auto inline_blur_radius=search_field_material::inline_blur_radius;
};
template<template<class> class Engine>using MaterialBinding=Engine<Policy>;
} // namespace lvui::native::text_field_material
