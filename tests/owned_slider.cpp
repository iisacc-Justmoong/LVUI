#include <LVUI/Slider.h>
#include <stdexcept>
int main(){lvui::SliderConfiguration c;if(c.width!=320||c.height!=32||c.value!=.5)throw std::runtime_error("Slider defaults changed");}
