#include <LVUI/ListFooter.h>
#include <stdexcept>
int main(){
    lvui::ListFooterConfiguration value;
    if(value.slots[0].icon_name!="addFile"||value.slots[1].icon_name!="generaldelete"||
       value.slots[2].icon_name!="settings"||!value.slots[2].menu_indicator||
       value.icon_size!=18||value.button_height!=22||!value.enabled||!value.interactive)
        throw std::runtime_error("List footer declaration values changed");
}
