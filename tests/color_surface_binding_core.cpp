#include <LVUI/NativeColorSurface.h>
#include <concepts>
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::ColorSurfaceBinding<Model,Host>;static_assert(std::same_as<B::Policy::Model,Model<lvui::ColorSurfaceModelPolicy>>);static_assert(std::same_as<B::Policy::Input,lvui::native::ColorSurfaceInput>);}
