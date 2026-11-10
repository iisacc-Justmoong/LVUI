#include <LVUI/ListItem.h>
#include <stdexcept>
int main(){lvui::ListItemConfiguration value;if(value.variant!=lvui::ListItemVariant::mini||value.label!="Label"||
    value.icon_size!=14||value.left_padding!=4||!value.enabled||!value.interactive||!value.selectable)
    throw std::runtime_error("List item declaration values changed");}
