#include <LVUI/HierarchyToolbar.h>
#include <concepts>
template<class P>struct Model{using Policy=P;};
int main(){using M=lvui::HierarchyToolbarModelBinding<Model>;static_assert(std::same_as<typename M::Policy::Item,lvui::HierarchyToolbarItem>);}
