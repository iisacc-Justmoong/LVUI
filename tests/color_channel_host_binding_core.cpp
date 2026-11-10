#include <LVUI/NativeColorChannel.h>
#include <concepts>
struct Ramp{};struct Surface{};struct Field{};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::ColorChannelBinding<Host,Ramp,Surface,Field>;
 static_assert(std::same_as<typename B::Policy::Field,Field>);static_assert(std::same_as<typename B::Policy::Input,lvui::native::ColorChannelInput>);}
