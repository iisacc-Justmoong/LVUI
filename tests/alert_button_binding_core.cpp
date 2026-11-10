#include <LVUI/NativeAlertButton.h>
#include <concepts>
#include <stdexcept>
struct Image{};struct Decoder{using Image=::Image;};struct Widget{};struct Color{double red,green,blue,alpha;};
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){
    using Binding=lvui::native::AlertButtonBinding<Model,Host,Model,Host,Decoder,Widget>;
    using Button=lvui::native::ButtonBinding<Model,Host,Decoder,Widget>;
    static_assert(std::same_as<Binding::Policy::Button,Button>);
    static_assert(std::same_as<Binding::Policy::Callbacks,lvui::native::NativeAlertButtonCallbacksValue<Widget>>);
    if(lvui::native::alert_button_color(Color{.1,.2,.3,.4})!=lvui::StyleColor{.1,.2,.3,.4})
        throw std::runtime_error("Alert button color contract changed");
}
