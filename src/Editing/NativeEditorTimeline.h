#pragma once
#include <LVUI/StyleTokens.h>
namespace lvui::native {
// The application supplies this palette to its selected graphics host.
template<class Palette>[[nodiscard]] inline Palette editor_timeline_palette()noexcept {
    Palette p;
    const auto set=[](auto& destination,StyleColor source){destination={source.red,source.green,source.blue,source.alpha};};
    set(p.surface,figma_defaults::panel_surface_solid);set(p.divider,figma_defaults::panel_surface_panel);
    set(p.text,figma_defaults::text_body_color);set(p.clip,figma_defaults::panel_surface_input);
    set(p.selected,figma_defaults::panel_surface_alt);set(p.accent,figma_defaults::accent_blue_default);return p;
}
} // namespace lvui::native
