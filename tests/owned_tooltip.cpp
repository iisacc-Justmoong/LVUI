#include <LVUI/Tooltip.h>
#include <stdexcept>
int main(){lvui::TooltipConfiguration c;if(c.maximum_width<=0)throw std::runtime_error("Invalid tooltip defaults");}
