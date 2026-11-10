#include <LVUI/HierarchyItem.h>
#include <concepts>
template<class P>struct Model{using Policy=P;};
int main(){using M=lvui::HierarchyItemModelBinding<Model>;static_assert(std::same_as<typename M::Policy::Configuration,lvui::HierarchyItemConfiguration>);}
