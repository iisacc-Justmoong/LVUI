#include <LVUI/NativeSegmentedControl.h>
#include <concepts>
#include <stdexcept>
namespace {
template<class P>struct Model{using Policy=P;};
template<class P>struct Host{using Policy=P;};
struct Method{};struct Registry{};struct Invoker{};
struct Color{double red,green,blue,alpha;};
}
int main(){
    using Binding=lvui::native::SegmentedControlBinding<Model,Host,Method,Registry,Invoker>;
    static_assert(std::same_as<Binding::Policy::Model,Model<lvui::native::SegmentedControlModelPolicy>>);
    static_assert(std::same_as<Binding::Policy::Configuration,lvui::SegmentedControlConfiguration>);
    if(lvui::native::segmented_color(Color{.1,.2,.3,.4})!=lvui::StyleColor{.1,.2,.3,.4})
        throw std::runtime_error("Segmented control color contract changed");
}
