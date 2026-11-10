#include <LVUI/NativeTextFieldMaterial.h>
#include <concepts>
#include <stdexcept>
template<class P>struct Engine{using Policy=P;};
int main(){
    using Binding=lvui::native::text_field_material::MaterialBinding<Engine>;
    static_assert(std::same_as<Binding::Policy::Color,lvui::StyleColor>);
    if(Binding::Policy::radius!=lvui::search_field_defaults::radius||
       Binding::Policy::fill_gradient[0].position!=lvui::search_field_material::fill_gradient[0].position)
        throw std::runtime_error("text field material design values changed");
}
