#include <LVUI/ListItem.h>
#include <stdexcept>
int main(){lvui::ListItemConfiguration value;value.value="512 GB";value.variant=lvui::ListItemVariant::navigation;
    if(!value.show_value||!value.value||*value.value!="512 GB"||value.value_width!=lvui::list_item_navigation_tokens::value_width)
        throw std::runtime_error("List item value declaration changed");}
