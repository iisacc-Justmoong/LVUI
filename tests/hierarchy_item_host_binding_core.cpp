#include <LVUI/NativeHierarchyItem.h>
#include <concepts>
struct Image{};template<class P>struct Model{};struct Decoder{using Image=::Image;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::HierarchyItemBinding<Host,Model,Decoder>;static_assert(std::same_as<typename B::Policy::Input,lvui::native::HierarchyItemInputValue<Image>>);}
