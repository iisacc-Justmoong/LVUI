#include <LVUI/NativeColorPicker.h>
#include <concepts>
struct Layout{};struct Surface{};struct Channel{};struct Values{};struct Button{};struct Image{};struct Color{};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::ColorPickerBinding<Host,Layout,Surface,Channel,Values,Button,Image,Color>;
 static_assert(std::same_as<typename B::Policy::Input,lvui::native::ColorPickerInput>);static_assert(std::same_as<typename B::Policy::Surface,Surface>);}
