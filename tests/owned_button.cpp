#include <LVUI/Button.h>
#include <stdexcept>
int main() {
    const auto icon=lvui::icon_button_configuration();
    if(!icon.icon_mode || icon.height!=22 || icon.icon_size!=18 || icon.horizontal_padding!=2 || icon.corner_radius!=8)
        throw std::runtime_error("Authored icon button defaults changed");
    const lvui::ButtonConfiguration value;
    if(value.palette.primary!=lvui::figma_defaults::accent_blue_default ||
       value.palette.normal!=lvui::figma_defaults::panel_surface_solid || !(value==value))
        throw std::runtime_error("Authored button palette or equality changed");
}
