#include <LVUI/HelpButton.h>
#include <stdexcept>
int main() {
    const auto value=lvui::help_button_configuration();
    if(value.enter_key_activation || value.text!="?" || value.accessibility_name!="Help" ||
       value.width!=21 || value.height!=21 || value.horizontal_padding!=4 || value.corner_radius!=100 ||
       value.font_size!=12 || value.font_weight!=600)
        throw std::runtime_error("Authored help button defaults changed");
}
