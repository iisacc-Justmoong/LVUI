#include <LVUI/NativeListView.h>
#include <concepts>
struct Drawing{};struct Field{};struct Image{};struct Builder{};struct Decoder{using Image=::Image;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::ListViewBinding<Host,Drawing,Field,Image,Decoder,Builder>;static_assert(std::same_as<typename B::Policy::Drawing,Drawing>);}
