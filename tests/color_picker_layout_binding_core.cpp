#include <LVUI/ColorPickerLayout.h>
#include <concepts>
template<class P>struct Layout{using Policy=P;};
int main(){using B=lvui::ColorPickerLayoutBinding<Layout>;static_assert(std::same_as<typename B::Policy::Configuration,lvui::ColorPickerConfiguration>);}
