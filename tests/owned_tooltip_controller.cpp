#include <LVUI/TooltipController.h>
#include <type_traits>
int main(){static_assert(std::has_virtual_destructor_v<lvui::TooltipPresentationPort>);}
