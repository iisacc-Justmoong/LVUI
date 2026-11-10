#include <LVUI/NativeListItemSelector.h>
#include <concepts>
struct Image{};struct Combo{};struct Menu{};struct Item{};
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::ListItemSelectorBinding<Model,Host,Combo,Menu,Item,Image>;
 static_assert(std::same_as<B::Policy::Model,Model<lvui::ListItemSelectorModelPolicy>>);
 static_assert(std::same_as<B::Policy::Input,lvui::native::ListItemSelectorInput>);
 static_assert(std::same_as<B::Policy::NativeComboBox,Combo>);
 static_assert(std::same_as<B::Policy::NativeMenu,Menu>);
 static_assert(std::same_as<B::Policy::NativeMenuItem,Item>);}
