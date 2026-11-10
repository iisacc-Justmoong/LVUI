#include <LVUI/TooltipController.h>
#include <concepts>
template<class P>struct Engine{using Policy=P;};struct Model{};
int main(){using M=lvui::TooltipModelBinding<Engine>;using C=lvui::TooltipControllerBinding<Engine,Model>;static_assert(std::same_as<typename M::Policy::Configuration,lvui::TooltipConfiguration>);static_assert(std::same_as<typename C::Policy::Model,Model>);}
