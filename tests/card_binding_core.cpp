#include <LVUI/NativeCard.h>
#include <concepts>
struct Image{};struct Factory{};struct Decoder{using Image=::Image;};template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::CardBinding<Model,Host,Decoder,Factory>;static_assert(std::same_as<B::Policy::Model,Model<lvui::CardModelPolicy>>);
 static_assert(std::same_as<B::Policy::Input,lvui::native::CardInputValue<Image>>);static_assert(std::same_as<B::Policy::Views,lvui::native::NativeCardViewsValue<Factory>>);}
