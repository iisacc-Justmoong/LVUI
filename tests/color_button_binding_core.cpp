#include <LVUI/NativeColorPickerButton.h>
#include <concepts>
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using Binding=lvui::native::ColorPickerButtonBinding<Model,Host>;
    static_assert(std::same_as<Binding::Policy::Model,Model<lvui::ColorPickerButtonModelPolicy>>);
    static_assert(std::same_as<Binding::Policy::Input,lvui::native::ColorPickerButtonInput>);
}
