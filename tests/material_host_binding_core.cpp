#include <LVUI/NativeMaterial.h>
#include <concepts>
struct Widget{};template<class P>struct Model{using Policy=P;};template<class P>struct Gradient{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using Binding=lvui::native::MaterialBinding<Model,Gradient,Host,Widget>;
    using M=Model<lvui::MaterialModelPolicy>;
    static_assert(std::same_as<Binding::Policy::Model,M>);
    static_assert(std::same_as<Binding::Policy::Gradient,Gradient<lvui::native::MaterialGradientPolicy<M>>>);
    static_assert(std::same_as<Binding::Policy::Options,lvui::MaterialConfiguration>);
}
