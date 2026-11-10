#include <LVUI/NativeMobileNavigation.h>
#include <concepts>
struct Model{};struct Motion{};struct Material{};struct Decoder{using Image=int;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::MobileNavigationBinding<Host,Model,Motion,Material,Decoder>;static_assert(std::same_as<typename B::Policy::Model,Model>);}
