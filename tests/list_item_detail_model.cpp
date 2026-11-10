#include <LVUI/ListItem.h>
#include <stdexcept>
int main(){lvui::ListItemConfiguration value;value.variant=lvui::ListItemVariant::detail;value.detail_date="Today";
    if(!value.show_detail_date||!value.show_detail_folders||!value.show_detail_tags||value.detail_date!="Today")
        throw std::runtime_error("List item detail declaration changed");}
