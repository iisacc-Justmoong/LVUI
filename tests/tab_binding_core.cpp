#include <LVUI/NativeTab.h>
#include <concepts>
struct Widget{};struct Snapshot{};struct Decoder{using Image=int;};template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::TabBinding<Model,Host,Decoder,Widget,Snapshot>;static_assert(std::same_as<B::Policy::Model,Model<lvui::TabModelPolicy>>);
 static_assert(std::same_as<B::Policy::Input,lvui::native::TabInput>);static_assert(std::same_as<B::Policy::GroupPort,lvui::native::TabGroupPortValue<Widget,Snapshot>>);}
