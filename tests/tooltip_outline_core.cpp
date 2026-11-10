#include <LVUI/TooltipOutline.h>
#include <concepts>
template<class P>struct Engine{using Policy=P;};
int main(){using E=lvui::TooltipOutlineBinding<Engine>;static_assert(std::same_as<typename E::Policy::Layout,lvui::TooltipLayout>);}
