#include <LVUI/NativeColorPickerValues.h>
#include <concepts>
struct Field{};template<class P>struct Host{using Policy=P;};
int main(){using B=lvui::native::ColorPickerValuesBinding<Host,Field>;
 static_assert(std::same_as<typename B::Policy::Field,Field>);static_assert(std::same_as<typename B::Policy::Input,lvui::native::ColorPickerValuesInput>);}
