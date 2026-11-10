#include <LVUI/NativeCollectionWidgets.h>
#include <concepts>
template<class P>struct Host{using Policy=P;};struct Navigation{};struct Viewport{};
int main(){using B=lvui::widgets::CollectionWidgetNavigationBinding<Host,int,Navigation,Viewport>;static_assert(std::same_as<B::Policy::Item,int>);static_assert(std::same_as<B::Policy::Navigation,Navigation>);}
