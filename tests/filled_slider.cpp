#include <LVUI/Slider.h>
#include <stdexcept>
int main(){lvui::SliderConfiguration c;lvui::apply_slider_type(c,lvui::SliderType::segmented);if(!c.show_ticks||!c.show_min_max)throw std::runtime_error("Segmented design defaults changed");}
