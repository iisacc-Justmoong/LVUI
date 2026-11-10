#include <LVUI/ListPanel.h>
#include <stdexcept>
int main(){lvui::ListPanelConfiguration value;if(value.width<=0||value.height<=0)throw std::runtime_error("Panel defaults are invalid");}
