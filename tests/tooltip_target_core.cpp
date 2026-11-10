#include <LVUI/NativeTooltipTarget.h>
#include <concepts>
struct Motion{};struct Model{};struct Controller{};struct Presentation{};struct Scheduler{};template<class P>struct Engine{using Policy=P;};
int main(){using E=lvui::native::TooltipTargetBinding<Engine,Motion,Model,Controller,Presentation,Scheduler>;static_assert(std::same_as<typename E::Policy::Scheduler,Scheduler>);static_assert(std::same_as<typename E::Policy::Input,lvui::native::TooltipInputValue<Motion>>);}
