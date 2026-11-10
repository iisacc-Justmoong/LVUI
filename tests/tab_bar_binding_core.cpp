#include <LVUI/NativeTabBar.h>
#include <concepts>
struct Widget{};struct Snapshot{};template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::TabBarBinding<Model,Host,Widget,Snapshot>;static_assert(std::same_as<B::Policy::Model,Model<lvui::TabBarModelPolicy>>);
 static_assert(std::same_as<B::Policy::GroupPort,lvui::native::TabGroupPortValue<Widget,Snapshot>>);}
