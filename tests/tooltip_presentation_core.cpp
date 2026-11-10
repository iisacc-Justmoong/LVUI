#include <LVUI/NativeTooltipPresentation.h>
#include <concepts>
struct Motion{};struct Model{};struct Material{};struct Outline{};template<class P>struct Engine{using Policy=P;};
int main(){using E=lvui::native::TooltipPresentationBinding<Engine,Motion,Model,Material,Outline>;static_assert(std::same_as<typename E::Policy::Material,Material>);}
