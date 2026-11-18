#include <LVUI/MenuItem.h>
#include <stdexcept>
int main(){
    lvui::MenuItemConfiguration value;
    if(value.label!="Label"||value.state!=lvui::MenuEntryState::normal||value.direction!=lvui::MenuDirection::automatic||
       !lvui::MenuItemModelPolicy::valid_text(value.label)||lvui::MenuItemModelPolicy::valid_text(std::string(1, '\0')))
        throw std::runtime_error("Menu item design values or text contract changed");
    if(value.palette.text!=lvui::figma_defaults::text_title_header_color ||
       value.palette.selected!=lvui::figma_defaults::accent_blue_selected ||
       value.palette.pressed!=value.palette.selected)
        throw std::runtime_error("Menu selection and press must use the LVUI selected fill");
}
