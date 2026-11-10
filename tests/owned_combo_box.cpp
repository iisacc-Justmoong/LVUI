#include <LVUI/ComboBox.h>
#include <stdexcept>
int main(){lvui::ComboBoxConfiguration c;if(c.width!=97||c.height!=20||c.corner_radius!=5||c.arrow!=lvui::StepperArrow::up_down)throw std::runtime_error("ComboBox design defaults changed");}
