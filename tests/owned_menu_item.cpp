#include <LVUI/MenuItem.h>
#include <stdexcept>
int main(){
    lvui::MenuItemConfiguration value;
    if(value.label!="Label"||value.state!=lvui::MenuEntryState::normal||value.direction!=lvui::MenuDirection::automatic||
       !lvui::MenuItemModelPolicy::valid_text(value.label)||lvui::MenuItemModelPolicy::valid_text(std::string(1, '\0')))
        throw std::runtime_error("Menu item design values or text contract changed");
}
