#include <LVUI/ToolbarButton.h>
#include <stdexcept>
int main(){
    lvui::ToolbarButtonConfiguration value;
    if(value.tone!=lvui::ButtonTone::borderless||value.height!=22||value.icon_size!=18||
       value.left_padding!=2||value.right_padding!=2||value.spacing!=4||!value.enabled)
        throw std::runtime_error("Toolbar button declaration values changed");
}
