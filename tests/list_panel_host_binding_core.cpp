#include <LVUI/NativeListPanel.h>
#include <concepts>
struct Layout{};struct ToolbarModel{};struct FooterModel{};struct List{};struct Toolbar{};struct Footer{};struct Editor{};struct Builder{};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::ListPanelBinding<Host,Layout,ToolbarModel,FooterModel,List,Toolbar,Footer,Editor,Builder>;static_assert(std::same_as<typename B::Policy::List,List>);}
