#include <LVUI/ListItemSelector.h>
#include <stdexcept>
int main(){lvui::ListItemSelectorConfiguration c;if(c.placeholder!="Option"||!c.options.empty()||c.selected_index!=0)
 throw std::runtime_error("Selector declaration defaults changed");}
