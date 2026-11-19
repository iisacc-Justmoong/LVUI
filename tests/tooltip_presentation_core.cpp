#include <LVUI/NativeTooltipPresentation.h>
#include <concepts>
#include <stdexcept>
struct Motion{};struct Model{};struct Material{};struct Outline{};template<class P>struct Engine{using Policy=P;};
int main(){
    using E=lvui::native::TooltipPresentationBinding<Engine,Motion,Model,Material,Outline>;
    static_assert(std::same_as<typename E::Policy::Material,Material>);
    const lvui::native::TooltipPresentationStyleValue<Motion> style;
    if (style.material.color != lvui::figma_defaults::panel_surface_ghost ||
        style.material.tint_opacity != 0.84 || style.material.intense_opacity != 0.02 ||
        style.material.faint_opacity != 0.0055 || style.material.blur_radius != 20 ||
        style.material.inner_highlight_opacity != 0.12)
        throw std::runtime_error("Tooltip glass defaults differ from the design");
}
