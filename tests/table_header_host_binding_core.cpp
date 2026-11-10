#include <LVUI/NativeTableHeader.h>
#include <concepts>
struct Viewport{};struct Model{};struct Presentation{};struct Group{};struct Builder{};struct Rect{};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::TableHeaderBinding<Host,Model,Presentation,Group,Builder,Rect,Viewport>;static_assert(std::same_as<typename B::Policy::Model,Model>);}
