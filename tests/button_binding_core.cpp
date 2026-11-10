#include <LVUI/NativeButton.h>
#include <concepts>
#include <stdexcept>
struct Image{};struct Decoder{using Image=::Image;};struct Widget{};
struct Color{double red,green,blue,alpha;};enum class Tone{primary,normal,borderless,destructive,disabled};
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){
    using Binding=lvui::native::ButtonBinding<Model,Host,Decoder,Widget>;
    using Policy=Binding::Policy;
    static_assert(std::same_as<Policy::Model,Model<lvui::native::ButtonHostModelPolicy>>);
    static_assert(std::same_as<Policy::Input,lvui::native::ButtonInputValue<Image>>);
    static_assert(std::same_as<Policy::Event,lvui::native::ButtonEventValue<Widget>>);
    if(lvui::native::button_color(Color{.1,.2,.3,.4})!=lvui::StyleColor{.1,.2,.3,.4}||
       lvui::native::button_tone(Tone::normal)!=lvui::ButtonTone::normal||!lvui::native::ButtonHostModelPolicy::valid_text("Button"))
        throw std::runtime_error("Independent button binding changed");
}
