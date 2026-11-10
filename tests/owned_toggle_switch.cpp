#include <LVUI/ToggleSwitch.h>
#include <stdexcept>
int main(){
    lvui::ToggleSwitchConfiguration value;
    if(value.track_width!=38 || value.track_height!=22 || value.knob_size!=18 || value.checked ||
       value.palette.on!=lvui::figma_defaults::accent_blue_default || !(value==value))
        throw std::runtime_error("Authored ToggleSwitch values changed");
}
