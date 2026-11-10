#include <LVUI/NativePageRouter.h>
#include <concepts>
struct Builder{};template<class P>struct Host{using Policy=P;};
int main(){using Binding=lvui::native::PageRouterBinding<Host,Builder>;
    static_assert(std::same_as<Binding::Policy::Page,lvui::native::NativeNavigationPageValue<Builder>>);
    static_assert(std::same_as<Binding::Policy::Router,lvui::PageRouter>);
    static_assert(std::derived_from<lvui::native::FunctionalNavigationPageValue<Builder>,lvui::NavigationPage>);
}
