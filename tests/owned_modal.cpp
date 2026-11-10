#include <LVUI/Modal.h>
#include <stdexcept>
int main(){lvui::ModalConfiguration value;
    if(value.accessibility_name!="Modal"||value.action_text[0]!="OK"||value.minimum_width!=520||
       value.maximum_width!=980||value.shape!=lvui::ModalShape::rounded||!value.dismiss_on_escape)
        throw std::runtime_error("Modal declaration values changed");}
