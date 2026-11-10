#include <LVUI/NativeModal.h>
#include <concepts>
struct Image{};struct Decoder{using Image=::Image;};struct Widget{};struct Builder{};struct Port{};
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){
    using Binding=lvui::native::ModalBinding<Model,Host,Model,Model,Host,Decoder,Widget,Builder,Port>;
    static_assert(std::same_as<Binding::Policy::Input,lvui::native::ModalInputValue<Image>>);
    static_assert(std::same_as<Binding::Policy::ContentFactory,lvui::native::ModalContentFactoryValue<Builder,Widget>>);
    static_assert(std::same_as<Binding::Policy::Model,Model<lvui::native::ModalHostModelPolicy>>);
}
