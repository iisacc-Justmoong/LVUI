#include <LVUI/NativeHierarchyList.h>
#include <concepts>
struct ItemModel{};struct Item{};struct Image{};struct Builder{};struct Decoder{using Image=::Image;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::HierarchyListBinding<Host,ItemModel,Item,Image,Decoder,Builder>;static_assert(std::same_as<typename B::Policy::ItemModel,ItemModel>);}
