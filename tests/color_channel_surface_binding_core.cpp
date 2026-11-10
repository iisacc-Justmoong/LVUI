#include <LVUI/ColorChannelSurface.h>
#include <concepts>
template<class P>struct Surface{using Policy=P;};
int main(){using B=lvui::ColorChannelSurfaceBinding<Surface>;static_assert(std::same_as<B::Policy,lvui::ColorChannelSurfacePolicy>);}
