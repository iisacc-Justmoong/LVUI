#include <LVUI/ListToolbar.h>
#include <stdexcept>
int main(){
    lvui::ListToolbarConfiguration value;
    if(value.icon_size!=18||value.button_size!=22||value.horizontal_padding!=8||
       value.vertical_padding!=4||value.spacing!=4||!value.enabled||!value.interactive)
        throw std::runtime_error("List toolbar declaration values changed");
}
