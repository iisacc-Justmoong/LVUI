#include <LVUI/Popover.h>
#include <stdexcept>
int main(){lvui::PopoverConfiguration value;
    if(value.padding!=12||value.margins!=8||!lvui::PopoverModelPolicy::valid_text(value.accessibility_name)||
       lvui::PopoverModelPolicy::valid_text(std::string(1,'\0')))throw std::runtime_error("Popover design contract changed");
}
