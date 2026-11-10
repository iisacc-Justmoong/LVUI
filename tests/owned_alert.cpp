#include <LVUI/Alert.h>
#include <stdexcept>
int main(){lvui::AlertConfiguration value;
    if(value.accessibility_name!="Alert"||value.minimum_width!=280||value.maximum_width!=500||
       value.shape!=lvui::AlertShape::rounded||!value.show_icon||value.dismiss_on_escape)
        throw std::runtime_error("Alert declaration values changed");}
