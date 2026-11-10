#include <LVUI/SegmentedControl.h>
#include <LVUI/Button.h>
#include <limits>
#include <stdexcept>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    const lvui::SegmentedControlConfiguration original;
    require(original.horizontal_padding==0&&original.vertical_padding==0&&original.spacing==2&&
        original.border_width==0&&original.corner_radius==8&&
        original.corner_radius==lvui::ButtonConfiguration{}.corner_radius&&original.force_borderless_tone,
        "Segmented control defaults changed");
    require(original.background==lvui::figma_defaults::panel_sub_surface &&
            original.border==lvui::figma_defaults::panel_surface_raised,
            "Label segmented surface does not follow the current Figma variables");
    const auto icon=lvui::icon_segmented_configuration();
    require(icon.horizontal_padding==0 && icon.vertical_padding==0 && icon.border_width==0 && icon.spacing==2 &&
            icon.corner_radius==original.corner_radius && icon.background==original.background && icon.border==original.border,
            "Icon segmented defaults do not follow their distinct Figma geometry");
}
