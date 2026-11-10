#include <LVUI/Tab.h>
#include <stdexcept>
int main(){lvui::TabConfiguration tab;lvui::TabBarConfiguration bar;if(tab.height!=32||bar.minimum_tab_width!=120)throw std::runtime_error("Tab design defaults changed");}
