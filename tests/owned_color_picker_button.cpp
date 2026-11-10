#include <LVUI/ColorPickerButton.h>
#include <stdexcept>
int main(){lvui::ColorPickerButtonConfiguration c;
    if(c.size!=28||!c.show_hue_ring||c.current_color!=lvui::figma_defaults::accent_purple_default||!lvui::ColorPickerButtonModelPolicy::valid_color(c.current_color))
        throw std::runtime_error("Color button design contract changed");
}
