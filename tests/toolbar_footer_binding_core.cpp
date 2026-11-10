#include <LVUI/NativeListFooter.h>
#include <concepts>
#include <stdexcept>
struct Image{};struct Decoder{using Image=::Image;};
struct Color{double red,green,blue,alpha;};
enum class Tone{primary,normal,borderless,destructive,disabled};
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){
    using Button=lvui::native::ToolbarButtonBinding<Model,Host,Decoder>;
    using Footer=lvui::native::ListFooterBinding<Model,Host,Model,Host,Decoder>;
    static_assert(std::same_as<Button::Policy::Input,lvui::native::ToolbarButtonInputValue<Image>>);
    static_assert(std::same_as<Footer::Policy::Input,lvui::native::ListFooterInput>);
    static_assert(std::same_as<Footer::Policy::Button,Button>);
    if(lvui::native::toolbar_button_color(Color{.1,.2,.3,.4})!=lvui::StyleColor{.1,.2,.3,.4}||
       lvui::native::toolbar_button_tone(Tone::destructive)!=lvui::ButtonTone::destructive||
       lvui::native::list_footer_color(Color{.1,.2,.3,.4})!=lvui::StyleColor{.1,.2,.3,.4})
        throw std::runtime_error("Toolbar and footer binding contract changed");
}
