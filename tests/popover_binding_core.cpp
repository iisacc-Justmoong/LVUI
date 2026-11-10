#include <LVUI/NativePopover.h>
#include <concepts>
template<class P>struct Model{using Policy=P;};template<class P>struct Gradient{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using Binding=lvui::native::PopoverBinding<Model,Model,Gradient,Host>;
    static_assert(std::same_as<Binding::Policy::Model,Model<lvui::PopoverModelPolicy>>);
    static_assert(std::same_as<Binding::Policy::MaterialModel,Model<lvui::MaterialModelPolicy>>);
    static_assert(std::same_as<Binding::Policy::Options,lvui::PopoverConfiguration>);
}
