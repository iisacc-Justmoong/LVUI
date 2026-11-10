#include <LVUI/NativeLink.h>
#include <concepts>
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::LinkBinding<Model,Host>;static_assert(std::same_as<B::Policy::Model,Model<lvui::LinkModelPolicy>>);
 static_assert(std::same_as<B::Policy::Options,lvui::LinkConfiguration>);static_assert(std::same_as<B::Policy::Context,lvui::NavigationContext>);}
