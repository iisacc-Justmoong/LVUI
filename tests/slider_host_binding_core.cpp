#include <LVUI/NativeSlider.h>
#include <concepts>
struct Decoder{using Image=int;};
template<class P> struct Model{using Policy=P;};
template<class P> struct Host{using Policy=P;};
int main(){using B=lvui::native::SliderViewBinding<Model,Host,Decoder>;
 static_assert(std::same_as<typename B::Policy::Configuration,lvui::SliderConfiguration>);
 static_assert(std::same_as<typename B::Policy::Model,Model<lvui::SliderModelPolicy>>);}
