#include <LVUI/NativeMaterialGradient.h>
#include <concepts>
template<class P>struct Cache{using Policy=P;};
int main(){using Binding=lvui::native::MaterialGradientBinding<Cache,int>;
    static_assert(std::same_as<Binding::Policy::Material,int>);
    static_assert(Binding::Policy::edge_rgb.size()==3);
}
