#include <LVUI/Link.h>
#include <stdexcept>
int main(){lvui::LinkConfiguration c;if(c.href!="/"||c.tone!=lvui::ButtonTone::borderless||c.minimum_height!=36)throw std::runtime_error("Link defaults changed");}
