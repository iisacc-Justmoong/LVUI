#include <LVUI/DropdownButton.h>
#include <stdexcept>
int main() {
    const lvui::DropdownConfiguration value;
    if(value.height!=22 || value.left_padding!=8 || value.right_padding!=2 || value.indicator_size!=18 ||
       value.corner_radius!=8 || !(value==value))
        throw std::runtime_error("Authored dropdown defaults or equality changed");
}
