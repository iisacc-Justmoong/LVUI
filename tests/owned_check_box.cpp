#include <LVUI/CheckBox.h>
#include <stdexcept>
int main(){
    lvui::CheckBoxConfiguration value;
    if(value.box_size!=17 || value.checked || value.palette.checked!=lvui::figma_defaults::accent_blue_default ||
       value.use_figma_checked_assets.has_value() || !(value==value))
        throw std::runtime_error("Authored CheckBox values changed");
}
