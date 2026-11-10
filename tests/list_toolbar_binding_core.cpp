#include <LVUI/NativeListToolbar.h>
#include <concepts>
#include <stdexcept>
struct Image{};struct Decoder{using Image=::Image;};struct Color{double red,green,blue,alpha;};
template<class P>struct Model{using Policy=P;};template<class P>struct Host{using Policy=P;};
int main(){
    using Binding=lvui::native::ListToolbarBinding<Model,Host,Decoder>;
    static_assert(std::same_as<Binding::Policy::Model,Model<lvui::native::ListToolbarModelPolicy>>);
    static_assert(std::same_as<Binding::Policy::Input,lvui::native::ListToolbarInput>);
    if(lvui::native::list_toolbar_color(Color{.1,.2,.3,.4})!=lvui::StyleColor{.1,.2,.3,.4}||
       !lvui::native::ListToolbarModelPolicy::valid_text("Home"))throw std::runtime_error("List toolbar binding changed");
}
