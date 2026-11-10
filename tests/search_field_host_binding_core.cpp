#include <LVUI/NativeSearchField.h>
#include <concepts>
struct Field{};struct Material{};struct Decoder{using Image=int;};struct Affinity{};
template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::SearchFieldBinding<Host,Field,Material,Decoder,Affinity>;
 static_assert(std::same_as<typename B::Policy::Field,Field>);
 static_assert(std::same_as<typename B::Policy::Configuration,lvui::SearchFieldConfiguration>);}
