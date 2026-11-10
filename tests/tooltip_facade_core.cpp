#include <LVUI/NativeTooltip.h>
#include <LVUI/NativeTooltipScheduler.h>
#include <concepts>
struct Motion{};struct Model{};struct Controller{};struct Presentation{};struct Scheduler{};struct Target{};template<class P>struct Engine{using Policy=P;};
int main(){using E=lvui::native::TooltipBinding<Engine,Motion,Model,Controller,Presentation,Scheduler,Target>;static_assert(std::same_as<typename E::Policy::Target,Target>);static_assert(std::same_as<lvui::native::TooltipSchedulerBinding<Scheduler>,Scheduler>);}
