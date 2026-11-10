#include <LVUI/NativeListItemSegments.h>
#include <concepts>
struct Image{};struct Button{};struct Group{};
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::ListItemSegmentsBinding<Model,Host,Button,Group,Image>;
 static_assert(std::same_as<B::Policy::Model,Model<lvui::ListItemSegmentsModelPolicy>>);
 static_assert(std::same_as<B::Policy::Input,lvui::native::ListItemSegmentsInput>);
 static_assert(std::same_as<B::Policy::NativeButton,Button>);
 static_assert(std::same_as<B::Policy::NativeSegmentedControl,Group>);}
