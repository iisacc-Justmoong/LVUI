#include <LVUI/RadioButton.h>
#include <stdexcept>
int main(){
    lvui::RadioButtonConfiguration value;
    if(value.indicator_size!=18||value.dot_size!=8||value.checked||value.palette.on!=lvui::figma_defaults::accent_blue_default)
        throw std::runtime_error("Authored radio values changed");
}
