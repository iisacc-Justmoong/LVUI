#include <LVUI/NativeHierarchyToolbar.h>
#include <concepts>
struct Model{};struct Layout{};struct ButtonModel{};struct Button{};struct Image{};struct Decoder{using Image=::Image;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::HierarchyToolbarBinding<Host,Model,Layout,ButtonModel,Button,Image,Decoder>;static_assert(std::same_as<typename B::Policy::Model,Model>);}
