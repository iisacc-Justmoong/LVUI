#include <LVUI/NativeAlert.h>
#include <concepts>
struct Image{};struct Decoder{using Image=::Image;};struct Widget{};struct Port{using ImageInput=int;};
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){
    using Binding=lvui::native::AlertBinding<Model,Host,Model,Host,Model,Host,Decoder,Widget,Port>;
    static_assert(std::same_as<Binding::Policy::Input,lvui::native::AlertInputValue<Image>>);
    static_assert(std::same_as<Binding::Policy::Callbacks,lvui::native::NativeAlertCallbacksValue<Widget>>);
    static_assert(std::same_as<Binding::Policy::Model,Model<lvui::native::AlertModelPolicy>>);
}
