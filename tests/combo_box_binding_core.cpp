#include <LVUI/NativeComboBox.h>
#include <concepts>
struct Widget{};template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){using Binding=lvui::native::ComboBoxBinding<Model,Host,Widget>;
    static_assert(std::same_as<Binding::Policy::Model,Model<lvui::ComboBoxModelPolicy>>);
    static_assert(std::same_as<Binding::Policy::Options,lvui::ComboBoxConfiguration>);
    static_assert(std::same_as<Binding::Policy::Event,lvui::native::ButtonEventValue<Widget>>);
}
