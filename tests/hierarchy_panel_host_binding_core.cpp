#include <LVUI/NativeHierarchyPanel.h>
#include <concepts>
struct Layout{};struct ToolbarLayout{};struct FooterModel{};struct List{};struct Toolbar{};struct Footer{};struct Builder{};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::HierarchyPanelBinding<Host,Layout,ToolbarLayout,FooterModel,List,Toolbar,Footer,Builder>;static_assert(std::same_as<typename B::Policy::List,List>);}
