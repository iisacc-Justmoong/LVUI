#include <LVUI/NativeListItem.h>
#include <concepts>
#include <stdexcept>
struct Image{};struct Decoder{using Image=::Image;};struct Converter{};struct Port{};
template<class P>struct Model{};template<class P>struct Drawing{};template<class P>struct Host{using Policy=P;};
int main(){
    using Binding=lvui::native::ListItemBinding<Model,Drawing,Host,Decoder,Converter,Port>;
    using Policy=Binding::Policy;static_assert(std::same_as<Policy::Input,lvui::native::ListItemInputValue<Image>>);
    Policy::Input input;input.configuration.label="Label";
    if(!Policy::valid_text(input.configuration.label)||Policy::display_text("Label")!="Label")
        throw std::runtime_error("independent list item binding failed");
}
