#include <LVUI/AlertButton.h>
#include <stdexcept>
int main(){
    lvui::AlertButtonConfiguration value;
    if(value.text!="Button"||value.tone!=lvui::ButtonTone::normal||value.height!=20||
       !value.enabled||value.dialog_style||!value.motion_enabled||value.enter_key_activation||
       value.normal_pressed!=lvui::figma_defaults::panel_surface_input)
        throw std::runtime_error("AlertButton declaration values changed");
}
